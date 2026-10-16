load(":bt_kernel.bzl", "define_bt_modules")

def define_pineapple():
    define_bt_modules(
        target = "pineapple",
        modules = [
            "btpower",
            "bt_fm_slim",
            "radio-i2c-rtc6226-qca",
        ],
        config_options = [
            "CONFIG_MSM_BT_POWER",
            "CONFIG_BTFM_SLIM",
            "CONFIG_I2C_RTC6226_QCA",
            "CONFIG_FMD_ENABLE",
            #"CONFIG_BT_HW_SECURE_DISABLE",
        ]
    )

def define_sun():
    define_bt_modules(
        target = "sun",
        modules = [
            "btpower",
            "radio-i2c-rtc6226-qca",
            "btfm_slim_codec",
            "btfmcodec",
            "bt_fm_swr",
            "spi_cnss_proto",
            "thqspi_proto",
         ],
         config_options = [
             "CONFIG_MSM_BT_POWER",
             "CONFIG_I2C_RTC6226_QCA",
             "CONFIG_SLIM_BTFM_CODEC",
             "CONFIG_BTFM_CODEC",
           #  "CONFIG_BT_HW_SECURE_DISABLE",
             "CONFIG_BTFM_SWR",
	     "CONFIG_FMD_ENABLE",
            "CONFIG_BTFM_SWR",
            "CONFIG_SPI_CNSS_PROTO",
             "CONFIG_THQSPI_PROTO",
        ]
    )

def define_parrot():
    define_bt_modules(
        target = "parrot66",
        modules = [
            "btpower",
            "bt_fm_slim",
            "radio-i2c-rtc6226-qca",
        ],
        config_options = [
            "CONFIG_MSM_BT_POWER",
            "CONFIG_BTFM_SLIM",
            "CONFIG_I2C_RTC6226_QCA",
            #"CONFIG_BT_HW_SECURE_DISABLE",
        ]
    )

def define_parrot66():
    define_bt_modules(
        target = "parrot",
        modules = [
            "btpower",
            "bt_fm_slim",
            "radio-i2c-rtc6226-qca",
        ],
        config_options = [
            "CONFIG_MSM_BT_POWER",
            "CONFIG_BTFM_SLIM",
            "CONFIG_I2C_RTC6226_QCA",
            #"CONFIG_BT_HW_SECURE_DISABLE",
        ]
    )

def define_canoe():
    define_bt_modules(
        target = "canoe",
        modules = [
            "btpower",
            "radio-i2c-rtc6226-qca",
            "btfm_slim_codec",
            "btfmcodec",
            "bt_fm_swr",
            "spi_cnss_proto",
            "thqspi_proto",
         ],
         config_options = [
             "CONFIG_MSM_BT_POWER",
             "CONFIG_I2C_RTC6226_QCA",
             "CONFIG_SLIM_BTFM_CODEC",
             "CONFIG_BTFM_CODEC",
           #  "CONFIG_BT_HW_SECURE_DISABLE",
             "CONFIG_BTFM_SWR",
             "CONFIG_FMD_ENABLE",
             "CONFIG_SPI_CNSS_PROTO",
             "CONFIG_THQSPI_PROTO",
             "CONFIG_EXT_BUCK",
        ]
    )

def define_chora():
    define_bt_modules(
        target = "chora",
        modules = [
            "btpower",
            "radio-i2c-rtc6226-qca",
            "btfm_slim_codec",
            "btfmcodec",
            "bt_fm_swr",
            "spi_cnss_proto",
            "thqspi_proto",
         ],
         config_options = [
             "CONFIG_MSM_BT_POWER",
             "CONFIG_I2C_RTC6226_QCA",
             "CONFIG_SLIM_BTFM_CODEC",
             "CONFIG_BTFM_CODEC",
           #  "CONFIG_BT_HW_SECURE_DISABLE",
             "CONFIG_BTFM_SWR",
             "CONFIG_FMD_ENABLE",
             "CONFIG_SPI_CNSS_PROTO",
             "CONFIG_THQSPI_PROTO",
             "CONFIG_EXT_BUCK",
        ]
    )
def define_malabar():
    define_bt_modules(
        target = "malabar",
        modules = [
            "btpower",
            "radio-i2c-rtc6226-qca",
            "btfm_slim_codec",
            "btfmcodec",
        #    "bt_fm_swr",
            "spi_cnss_proto",
            "thqspi_proto",
         ],
         config_options = [
             "CONFIG_MSM_BT_POWER",
             "CONFIG_I2C_RTC6226_QCA",
             "CONFIG_SLIM_BTFM_CODEC",
             "CONFIG_BTFM_CODEC",
           #  "CONFIG_BT_HW_SECURE_DISABLE",
           #  "CONFIG_BTFM_SWR",
             "CONFIG_SPI_CNSS_PROTO",
             "CONFIG_THQSPI_PROTO",
        ]
    )

def define_seraph():
    define_bt_modules(
        target = "seraph",
        modules = [
            "btpower",
            "btfmcodec",
            "bt_fm_swr",
         ],
         config_options = [
             "CONFIG_MSM_BT_POWER",
             "CONFIG_BTFM_CODEC",
             "CONFIG_BTFM_SWR",
        ]
    )

def define_bengal():
    define_bt_modules(
        target = "bengal",
        modules = [
            "btpower",
            "bt_fm_slim",
            "radio-i2c-rtc6226-qca",
            "btfmcodec",
         ],
         config_options = [
             "CONFIG_MSM_BT_POWER",
             "CONFIG_BTFM_SLIM",
             "CONFIG_I2C_RTC6226_QCA",
             "CONFIG_BTFM_CODEC",
        ]
    )

def define_hamoa():
    define_bt_modules(
       target = "hamoa",
       modules = [
           "btpower",
           "radio-i2c-rtc6226-qca",
           "bt_fm_slim",
        ],
    config_options = [
        "CONFIG_MSM_BT_POWER",
        "CONFIG_I2C_RTC6226_QCA",
        "CONFIG_BTFM_SLIM",
    ]
  )
def define_lahaina():
    define_bt_modules(
       target = "lahaina",
       modules = [
           "btpower",
           "bt_fm_slim",
           "radio-i2c-rtc6226-qca",
        ],
    config_options = [
       "CONFIG_MSM_BT_POWER",
       "CONFIG_BTFM_SLIM",
       "CONFIG_I2C_RTC6226_QCA",
    ]
  )

def define_hamoa_la():
    define_bt_modules(
        target = "hamoa_la",
        modules = [
            "btpower",
            "radio-i2c-rtc6226-qca",
         ],
         config_options = [
             "CONFIG_MSM_BT_POWER",
             "CONFIG_I2C_RTC6226_QCA",
             # "CONFIG_FMD_ENABLE",  # Commented out - cnss_utils dependency not available for hamoa_la
        ]
    )
