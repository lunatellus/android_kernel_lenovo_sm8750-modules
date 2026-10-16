// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "qti-smmu-proxy-common.h"

#include <linux/qcom-iommu-util.h>
#include <linux/qti-smmu-proxy-callbacks.h>
#include <linux/qcom-dma-mapping.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/pm_wakeup.h>

#define DELAY_MS 30
#define GH_MSGQ_RECV_RETRY_CNT 10

static void *msgq_hdl;

DEFINE_MUTEX(sender_mutex);

static const struct file_operations smmu_proxy_dev_fops;

/*
 * During PVM suspend, the proxy scheduler threads get frozen, which means the
 * VM will not reply to any messages smmu-proxy sends. This driver blocks
 * suspend to prevent this, although since suspend 'polls' pm_wakeup_pending()
 * the above situation may still temporarily occur.
 */
static struct device *smmu_proxy_pvm_dev;

/*
 * req and resp may alias the same buffer. req_size bytes are sent
 * first (synchronously), then the buffer is overwritten with the response.
 * Callers must ensure the buffer is at least GH_MSGQ_MAX_MSG_SIZE_BYTES.
 */
static int smmu_proxy_send_msg(void *msgq_hdl, void *req, size_t req_size,
			       void *resp, u32 rsp_msg_type, size_t *resp_size)
{
	size_t size = 0;
	int ret;
	int retry_cnt;
	unsigned long flags = 0;
	const struct smmu_proxy_resp_hdr *resp_hdr = resp;
	u32 req_msg_type = ((struct smmu_proxy_msg_hdr *)req)->msg_type;

	pm_stay_awake(smmu_proxy_pvm_dev);

	ret = gh_msgq_send(msgq_hdl, req, req_size, 0);
	if (ret < 0) {
		pr_err("%s: failed to send message for request message type: %u rc: %d\n",
		       __func__, req_msg_type, ret);
		goto out;
	}

	/*
	 * No need to validate size - gh_msgq_recv_killable() ensures that
	 * size <= GH_MSGQ_MAX_MSG_SIZE_BYTES.
	 */
	retry_cnt = GH_MSGQ_RECV_RETRY_CNT;
	do {
		ret = gh_msgq_recv_killable(msgq_hdl, resp, GH_MSGQ_MAX_MSG_SIZE_BYTES,
					    &size, flags);

		/*
		 * A non-negative return value means a message was received from
		 * the queue. Validate the response contents before treating the
		 * request as complete.
		 */
		if (ret >= 0) {
			/*
			 * A valid response buffer must contain a response header.
			 * Without it, the caller cannot identify the message type
			 * or remote VM status.
			 */
			if (resp_hdr == NULL) {
				pr_err_ratelimited("%s: call failed with invalid response for msg type: %u rc: %d\n",
						   __func__, req_msg_type, ret);
				ret = -EINVAL;
				goto out;
			}

			/*
			 * The remote request returned an error for the request. Treat
			 * this as a failed request call.
			 */
			if (resp_hdr->ret) {
				pr_err_ratelimited("%s: call failed on remote VM for msg type: %u rc: %d\n",
						   __func__, req_msg_type, resp_hdr->ret);
				ret = -EINVAL;
				goto out;
			}

			/*
			 * The queue may contain stale or unrelated responses. If
			 * the message type does not match the expected response,
			 * switch to non-blocking receives and continue draining
			 * until the expected response is found or retries expire.
			 */
			if (resp_hdr->msg_type != rsp_msg_type) {
				pr_err("%s: received incorrect msg (type: %d) expected msg type: %u\n",
				       __func__, resp_hdr->msg_type, rsp_msg_type);
				flags = GH_MSGQ_NONBLOCK;
				ret = -EINVAL;
			} else {
				/*
				 * The expected response was received and passed
				 * validation, so the request completed successfully.
				 */
				ret = 0;

				/*
				 * Report the actual response size to the caller so it can
				 * verify the response payload matches the expected format.
				 */
				if (resp_size)
					*resp_size = size;
				goto out;
			}
		}

		if (retry_cnt == 1) {
			pr_err_ratelimited("%s: failed to receive message for type: %u rc: %d\n",
					   __func__, req_msg_type, ret);
			goto out;
		}
		pr_err_ratelimited("%s: failed to receive message for msg type: %u rc: %d, retry\n",
				   __func__, req_msg_type, ret);
		mdelay(DELAY_MS);
	} while (--retry_cnt);
out:
	pm_relax(smmu_proxy_pvm_dev);
	return ret;
}

int smmu_proxy_unmap(void *data)
{
	struct dma_buf *dmabuf;
	void *buf;
	int ret;
	struct smmu_proxy_unmap_req *req;
	struct smmu_proxy_unmap_resp *resp;
	size_t size = 0;

	mutex_lock(&sender_mutex);

	buf = kzalloc(GH_MSGQ_MAX_MSG_SIZE_BYTES, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		pr_err("%s: Failed to allocate memory!\n", __func__);
		goto out;
	}
	req = buf;

	dmabuf = data;
	ret = mem_buf_dma_buf_get_memparcel_hdl(dmabuf, &req->hdl);
	if (ret) {
		pr_err("%s: Failed to get memparcel handle rc: %d\n", __func__, ret);
		goto free_buf;
	}

	req->hdr.msg_type = SMMU_PROXY_UNMAP;
	req->hdr.msg_size = sizeof(*req);

	resp = buf;
	ret = smmu_proxy_send_msg(msgq_hdl, req, req->hdr.msg_size, resp,
				  SMMU_PROXY_UNMAP_RESP, &size);
	if (ret)
		goto free_buf;

	if (size != sizeof(*resp)) {
		pr_err_ratelimited("%s: Unmap call failed with invalid response size: %zu\n",
				   __func__, size);
		ret = -EINVAL;
		goto free_buf;
	}

free_buf:
	kfree(buf);
out:
	mutex_unlock(&sender_mutex);

	return ret;
}

int smmu_proxy_switch_sid(struct device *client_dev, u32 op)
{
	void *buf;
	int ret;
	struct smmu_proxy_switch_sid_req *req;
	struct smmu_proxy_switch_sid_resp *resp;
	size_t size = 0;
	u32 req_cb_id;

	mutex_lock(&sender_mutex);
	buf = kzalloc(GH_MSGQ_MAX_MSG_SIZE_BYTES, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		pr_err("%s: Failed to allocate memory!\n", __func__);
		goto out;
	}

	req = buf;

	req->hdr.msg_type = SMMU_PROXY_SWITCH_SID;
	req->hdr.msg_size = sizeof(*req);
	ret = of_property_read_u32(client_dev->of_node,
				   "qti,smmu-proxy-cb-id",
				   &req->cb_id);
	if (ret) {
		dev_err(client_dev, "%s: Err reading 'qti,smmu-proxy-cb-id' rc: %d\n",
			__func__, ret);
		goto free_buf;
	}
	req_cb_id = req->cb_id;

	switch (op) {
	case SMMU_PROXY_SWITCH_OP_RELEASE_SID:
		req->switch_dir = SID_RELEASE;
		break;
	case SMMU_PROXY_SWITCH_OP_ACQUIRE_SID:
		req->switch_dir = SID_ACQUIRE;
		break;
	default:
		ret = -EINVAL;
		goto free_buf;
	}

	resp = buf;
	ret = smmu_proxy_send_msg(msgq_hdl, req, req->hdr.msg_size, resp,
				  SMMU_PROXY_SWITCH_SID_RESP, &size);
	if (ret) {
		pr_err_ratelimited("%s: SID Switch call failed for cb_id: %u rc: %d\n",
				   __func__, req_cb_id, ret);
		goto free_buf;
	}

	if (size != sizeof(*resp)) {
		pr_err_ratelimited("%s: SID Switch call failed for cb_id: %u with invalid response size: %zu\n",
				   __func__, req_cb_id, size);
		ret = -EINVAL;
		goto free_buf;
	}

free_buf:
	kfree(buf);
out:
	mutex_unlock(&sender_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(smmu_proxy_switch_sid);

int smmu_proxy_map(struct device *client_dev, struct sg_table *proxy_iova,
		   struct dma_buf *dmabuf)
{
	void *buf;
	int ret = 0;
	int n_acl_entries, i;
	int vmids[2] = { VMID_TVM, VMID_OEMVM };
	int perms[2] = { PERM_READ | PERM_WRITE, PERM_READ | PERM_WRITE};
	struct csf_version csf_version;
	struct mem_buf_lend_kernel_arg arg = {0};
	struct smmu_proxy_map_req *req;
	struct smmu_proxy_map_resp *resp;
	size_t size = 0;

	ret = smmu_proxy_get_csf_version(&csf_version);
	if (ret) {
		return ret;
	}

	/*
	 * We enter this function iff the CSF version is 2.5.* . If CSF 2.5.1
	 * is in use, we set n_acl_entries to two, in order to assign this
	 * memory to the TVM and OEM VM. If CSF 2.5.0 is in use, we just assign
	 * it to the TVM.
	 */
	n_acl_entries = csf_version.min_ver == 1 ? 2 : 1;

	mutex_lock(&sender_mutex);

	buf = kzalloc(GH_MSGQ_MAX_MSG_SIZE_BYTES, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		pr_err("%s: Failed to allocate memory!\n", __func__);
		goto out;
	}

	if (mem_buf_dma_buf_exclusive_owner(dmabuf)) {
		arg.vmids = vmids;
		arg.perms = perms;
		arg.nr_acl_entries = n_acl_entries;

		ret = mem_buf_lend(dmabuf, &arg);
		if (ret) {
			pr_err("%s: Failed to lend buf rc: %d\n", __func__, ret);
			goto free_buf;
		}
	}
	req = buf;
	/* Prepare the message */
	req->acl_desc.n_acl_entries = n_acl_entries;
	for (i = 0; i < n_acl_entries; i++) {
		req->acl_desc.acl_entries[i].vmid = vmids[i];
		req->acl_desc.acl_entries[i].perms = perms[i];
	}

	ret = mem_buf_dma_buf_get_memparcel_hdl(dmabuf, &req->hdl);
	if (ret) {
		pr_err("%s: Failed to get memparcel handle rc: %d\n", __func__, ret);
		goto free_buf;
	}

	ret = of_property_read_u32(client_dev->of_node,
				   "qti,smmu-proxy-cb-id",
				   &req->cb_id);
	if (ret) {
		dev_err(client_dev, "%s: Err reading 'qti,smmu-proxy-cb-id' rc: %d\n",
			__func__, ret);
		goto free_buf;
	}

	req->hdr.msg_type = SMMU_PROXY_MAP;
	req->hdr.msg_size = offsetof(struct smmu_proxy_map_req,
			acl_desc.acl_entries[n_acl_entries]);

	resp = buf;
	ret = smmu_proxy_send_msg(msgq_hdl, req, req->hdr.msg_size, resp,
				  SMMU_PROXY_MAP_RESP, &size);
	if (ret)
		goto free_buf;

	if (size != sizeof(*resp)) {
		pr_err_ratelimited("%s: Map call failed with invalid response size: %zu\n",
				   __func__, size);
		ret = -EINVAL;
		goto free_buf;
	}

	ret = mem_buf_dma_buf_set_destructor(dmabuf, smmu_proxy_unmap, dmabuf);
	if (ret) {
		pr_err_ratelimited("%s: Failed to set vmperm destructor, rc: %d\n",
				   __func__, ret);
		goto free_buf;
	}

	sg_dma_address(proxy_iova->sgl) = resp->iova;
	sg_dma_len(proxy_iova->sgl) = resp->mapping_len;
	/*
	 * We set the number of entries to one here, as we only allow the mapping to go
	 * through on the TVM if the sg_table returned by dma_buf_map_attachment has one
	 * entry.
	 */
	proxy_iova->nents = 1;

free_buf:
	kfree(buf);
out:
	mutex_unlock(&sender_mutex);

	return ret;
}

void smmu_proxy_unmap_nop(struct device *client_dev, struct sg_table *table,
			  struct dma_buf *dmabuf)
{

}


static long smmu_proxy_dev_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	unsigned int dir = _IOC_DIR(cmd);
	union smmu_proxy_ioctl_arg ioctl_arg;
	int ret;

	if (_IOC_SIZE(cmd) > sizeof(ioctl_arg))
		return -EINVAL;

	if (copy_from_user(&ioctl_arg, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	if (!(dir & _IOC_WRITE))
		memset(&ioctl_arg, 0, sizeof(ioctl_arg));

	switch (cmd) {
	case QTI_SMMU_PROXY_GET_VERSION_IOCTL:
	{
		struct csf_version *csf_version =
			&ioctl_arg.csf_version;

		ret = smmu_proxy_get_csf_version(csf_version);
		if(ret)
			return ret;

		break;
	}

	default:
		return -ENOTTY;
	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &ioctl_arg,
				 _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	return 0;
}

static const struct file_operations smmu_proxy_dev_fops = {
	.unlocked_ioctl = smmu_proxy_dev_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};

static int sender_probe_handler(struct platform_device *pdev)
{
	int ret;
	struct csf_version csf_version;
	struct device *dev = &pdev->dev;

	msgq_hdl = gh_msgq_register(GH_MSGQ_LABEL_SMMU_PROXY);
	if (IS_ERR(msgq_hdl)) {
		ret = PTR_ERR(msgq_hdl);
		pr_err("%s: Queue registration failed rc: %ld!\n", __func__, PTR_ERR(msgq_hdl));
		return ret;
	}

	ret = smmu_proxy_get_csf_version(&csf_version);
	if (ret) {
		pr_err("%s: Failed to get CSF version rc: %d\n", __func__, ret);
		goto free_msgq;
	}

	if (csf_version.arch_ver == 2 && csf_version.max_ver == 0) {
		ret = qti_smmu_proxy_register_callbacks(NULL, NULL);
	} else if (csf_version.arch_ver == 2 && csf_version.max_ver == 5) {
		ret = qti_smmu_proxy_register_callbacks(smmu_proxy_map, smmu_proxy_unmap_nop);
	} else {
		pr_err("%s: Invalid CSF version: %d.%d\n", __func__, csf_version.arch_ver,
			csf_version.max_ver);
		goto free_msgq;
	}

	if (ret) {
		pr_err("%s: Failed to set SMMU proxy callbacks rc: %d\n", __func__, ret);
		goto free_msgq;
	}

	ret = device_init_wakeup(dev, true);
	if (ret) {
		dev_err(dev, "Failed to register wake-up source!\n");
		goto set_callbacks_null;
	}
	ret = smmu_proxy_create_dev(&smmu_proxy_dev_fops);
	if (ret) {
		pr_err("%s: Failed to create character device rc: %d\n", __func__,
		       ret);
		goto free_ws;
	}

	smmu_proxy_pvm_dev = dev;
	return 0;

free_ws:
	device_init_wakeup(dev, false);
set_callbacks_null:
	qti_smmu_proxy_register_callbacks(NULL, NULL);
free_msgq:
	gh_msgq_unregister(msgq_hdl);
	return ret;
}

static const struct of_device_id smmu_proxy_match_table[] = {
	{.compatible = "smmu-proxy-sender"},
	{},
};

static struct platform_driver smmu_proxy_driver = {
	.probe = sender_probe_handler,
	.driver = {
		.name = "qti-smmu-proxy",
		.of_match_table = smmu_proxy_match_table,
	},
};

int __init init_smmu_proxy_driver(void)
{
	return platform_driver_register(&smmu_proxy_driver);
}
module_init(init_smmu_proxy_driver);

#if (KERNEL_VERSION(6, 13, 0) <= LINUX_VERSION_CODE)
MODULE_IMPORT_NS("DMA_BUF");
#else
MODULE_IMPORT_NS(DMA_BUF);
#endif

MODULE_LICENSE("GPL v2");
