#include "svc_console.h"

#include "esp_console.h"
#include "esp_log.h"

static const char *TAG = "svc_console";

esp_err_t svc_console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "indicator> ";
    repl_cfg.max_cmdline_length = 256;

    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_err_t err = esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl);
    if (err != ESP_OK) return err;

    esp_console_register_help_command();

    err = esp_console_start_repl(repl);
    if (err == ESP_OK) ESP_LOGI(TAG, "REPL on UART0 @115200 (type 'help')");
    return err;
}
