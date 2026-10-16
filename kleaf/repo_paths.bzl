# SPDX-License-Identifier: GPL-2.0-only

"""Paths relative to the main platform workspace."""

SOC_REPO_PATH = "vendor/qcom/sm8850"
SOC_MODULES_REPO_PATH = "vendor/qcom/sm8850-modules"

def soc_label(target):
    """Return a label in the configured SoC kernel package."""
    return "//{}:{}".format(SOC_REPO_PATH, target)

def modules_path(path):
    """Return a workspace-relative path within the modules repository."""
    return "{}/{}".format(SOC_MODULES_REPO_PATH, path)

def modules_label(target):
    """Return a label within the configured modules repository."""
    return "//{}".format(modules_path(target))
