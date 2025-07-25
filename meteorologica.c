#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "aht20.h"
#include "bmp280.h"
#include "ssd1306.h"
#include "font.h"
#include <math.h>

#include <string.h>              // Biblioteca manipular strings
#include <stdlib.h>              // funções para realizar várias operações, incluindo alocação de memória dinâmica (malloc)

#include "pico/stdlib.h"         // Biblioteca da Raspberry Pi Pico para funções padrão (GPIO, temporização, etc.)
#include "hardware/adc.h"        // Biblioteca da Raspberry Pi Pico para manipulação do conversor ADC
#include "pico/cyw43_arch.h"     // Biblioteca para arquitetura Wi-Fi da Pico com CYW43  

#include "lwip/pbuf.h"           // Lightweight IP stack - manipulação de buffers de pacotes de rede
#include "lwip/tcp.h"            // Lightweight IP stack - fornece funções e estruturas para trabalhar com o protocolo TCP
#include "lwip/netif.h"          // Lightweight IP stack - fornece funções e estruturas para trabalhar com interfaces de rede (netif)

#include <hardware/pio.h>           
#include "hardware/clocks.h"        
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "animacao_matriz.pio.h" // Biblioteca PIO para controle de LEDs WS2818B
#include "credenciais_wifi.h" // Altere o arquivo de exemplo dentro do lib para suas credenciais e retire example do nome
#include "public/html_data.h"

#define I2C_PORT i2c0               // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C_SDA 0                   // 0 ou 2
#define I2C_SCL 1                   // 1 ou 3
#define SEA_LEVEL_PRESSURE 102025.0 // Pressão ao nível do mar em Pa
// Display na I2C
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C

// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN   // GPIO do CI CYW43
#define LED_PIN_GREEN 11
#define LED_PIN_BLUE 12
#define LED_PIN_RED 13
#define LED_COUNT 25            // Número de LEDs na matriz
#define MATRIZ_PIN 7            // Pino GPIO conectado aos LEDs WS2818B
#define BUZZER_A 21
#define JOY_X 27 // Joystick está de lado em relação ao que foi dito no pdf
#define JOY_Y 26
#define max_value_joy 4065.0 // (4081 - 16) que são os valores extremos máximos lidos pelo meu joystick
#define BOTAO_B 6

// Declaração de variáveis globais
PIO pio;
uint sm;
ssd1306_t ssd; // Inicializa a estrutura do display

uint padrao_led[10][LED_COUNT] = {
     // Desenho VERDE (segurança): círculo verde no centro
    {0, 0, 1, 0, 0,
     0, 1, 1, 1, 0,
     1, 1, 1, 1, 1,
     0, 1, 1, 1, 0,
     0, 0, 1, 0, 0,
    },
    // Desenho VERMELHO (erro): X vermelho
    {2, 0, 0, 0, 2,
     0, 2, 0, 2, 0,
     0, 0, 2, 0, 0,
     0, 2, 0, 2, 0,
     2, 0, 0, 0, 2,
    },
    {0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
    }, // Desliga os LEDs

};

// Ordem da matriz de LEDS, útil para poder visualizar na matriz do código e escrever na ordem correta do hardware
int ordem[LED_COUNT] = {0, 1, 2, 3, 4, 9, 8, 7, 6, 5, 10, 11, 12, 13, 14, 19, 18, 17, 16, 15, 20, 21, 22, 23, 24};  


struct http_state
{
    char response[15000];
    size_t len;
    size_t sent;
};

// Struct para armazenar valores já com offset aplicado
typedef struct {
    float temp_aht20;
    float hum_aht20;
    float temp_bmp280;
    float press_bmp280;
} MedidasCorrigidas;

MedidasCorrigidas medidas_corrigidas;

// Adicione as variáveis globais para limites e offsets
float min_temp_limit = 10.0, max_temp_limit = 35.0;
float min_press_limit = 950.0, max_press_limit = 1050.0;
float min_hum_limit = 20.0, max_hum_limit = 80.0;

// Offsets de calibração
float temp_offset = 0.0, press_offset = 0.0, hum_offset = 0.0;


// Estrutura para armazenar os dados do sensor
AHT20_Data data;
int32_t raw_temp_bmp;
int32_t raw_pressure;

int32_t temperature; // Variável para armazenar a temperatura convertida
int32_t pressure;    // Variável para armazenar a pressão convertida
double altitude;    // Variável para armazenar a altitude calculada

char str_tmp1[5];  // Buffer para armazenar a string
char str_press[10];  // Buffer para armazenar a string
char str_tmp2[5];  // Buffer para armazenar a string
char str_umi[5];  // Buffer para armazenar a string


// Prototipagem das funções
uint32_t matrix_rgb(unsigned r, unsigned g, unsigned b);
void display_desenho(int number);
void pwm_setup(uint pin);
void iniciar_buzzer(uint pin);
void parar_buzzer(uint pin);
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static void start_http_server(void);


// Função para calcular a altitude a partir da pressão atmosférica
double calculate_altitude(double pressure){
    return 44330.0 * (1.0 - pow(pressure / SEA_LEVEL_PRESSURE, 0.1903));
}


void gpio_irq_handler(uint gpio, uint32_t events){
    if (gpio == BOTAO_B) {
        absolute_time_t agora = get_absolute_time();
        static absolute_time_t ultimo_acionamento = 0;
        if (absolute_time_diff_us(ultimo_acionamento, agora) > 300000) {
            // Reseta os limites e offsets para valores padrão
            min_temp_limit = 10.0;
            max_temp_limit = 35.0;
            temp_offset = 0.0;
            min_hum_limit = 20.0;
            max_hum_limit = 80.0;
            hum_offset = 0.0;
            min_press_limit = 950.0;
            max_press_limit = 1050.0;
            press_offset = 0.0;
            ultimo_acionamento = agora;
        }
    }
}

int main(){
    sleep_ms(200); // Pequeno delay para garantir estabilidade

    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // I2C do Display funcionando em 400Khz.
    i2c_init(I2C_PORT_DISP, 400 * 1000);

    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA_DISP);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL_DISP);                                        // Pull up the clock line
    ssd1306_t ssd;                                                     // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT_DISP); // Inicializa o display
    ssd1306_config(&ssd);                                              // Configura o display
    ssd1306_send_data(&ssd);                                           // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicializa o I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa o BMP280
    bmp280_init(I2C_PORT);
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C_PORT, &params);

    // Inicializa o AHT20
    aht20_reset(I2C_PORT);
    aht20_init(I2C_PORT);
    // --- Leitura inicial 
    bmp280_read_raw(I2C_PORT, &raw_temp_bmp, &raw_pressure);
    temperature = bmp280_convert_temp(raw_temp_bmp, &params);
    pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);
    aht20_read(I2C_PORT, &data);

    stdio_init_all(); // Inicializa a saída padrão (UART)


    if (cyw43_arch_init()){
            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, "WiFi => FALHA", 0, 0);
            ssd1306_send_data(&ssd);

            return 1; // Retorna erro se não conseguir inicializar o Wi-Fi
        }

        cyw43_arch_enable_sta_mode();
        if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)){
            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, "WiFi => ERRO", 0, 0);
            ssd1306_send_data(&ssd);

            return 1; // Retorna erro se não conseguir conectar ao Wi-Fi
        }

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    printf("IP: %s\n",ip_str);
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "WiFi => OK", 0, 0);
    ssd1306_draw_string(&ssd, ip_str, 0, 10);
    ssd1306_send_data(&ssd);
    start_http_server();
    sleep_ms(2000); // pra dar tempo de ver o ip
    

    // Configuração do PIO
    pio = pio0; 
    uint offset = pio_add_program(pio, &animacao_matriz_program);
    sm = pio_claim_unused_sm(pio, true);
    animacao_matriz_program_init(pio, sm, offset, MATRIZ_PIN);

    // Configura os leds
    gpio_init(LED_PIN_RED);
    gpio_set_dir(LED_PIN_RED, GPIO_OUT);
    gpio_put(LED_PIN_RED, 0); // Desliga o LED vermelho
    gpio_init(LED_PIN_GREEN);
    gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
    gpio_put(LED_PIN_GREEN, 1); // Liga o LED verde

    

    
        
    bool cor = true;
    while(1){
        // Leitura do BMP280
        bmp280_read_raw(I2C_PORT, &raw_temp_bmp, &raw_pressure);
        temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);

        // Aplica offsets e armazena em struct
        medidas_corrigidas.temp_aht20 = data.temperature + temp_offset;
        medidas_corrigidas.hum_aht20 = data.humidity + hum_offset;
        medidas_corrigidas.temp_bmp280 = (temperature / 100.0) + temp_offset;
        medidas_corrigidas.press_bmp280 = (pressure / 100.0) + press_offset;

        // Cálculo da altitude
        altitude = calculate_altitude(medidas_corrigidas.press_bmp280 * 100.0); // press_bmp280 está em hPa, função espera Pa

        printf("Pressao = %.3f kPa\n", medidas_corrigidas.press_bmp280 / 10.0);
        printf("Temperatura BMP: = %.2f C\n", medidas_corrigidas.temp_bmp280);
        printf("Altitude estimada: %.2f m\n", altitude);

        // Leitura do AHT20
        if (aht20_read(I2C_PORT, &data))
        {
            printf("Temperatura AHT: %.2f C\n", medidas_corrigidas.temp_aht20);
            printf("Umidade: %.2f %%\n\n\n", medidas_corrigidas.hum_aht20);
        }
        else
        {
            printf("Erro na leitura do AHT10!\n\n\n");
        }

        sprintf(str_tmp1, "%.1fC", medidas_corrigidas.temp_bmp280);  // Converte o inteiro em string
        sprintf(str_press, "%.0fhPa", medidas_corrigidas.press_bmp280);  // Converte pressão em hPa para string
        sprintf(str_tmp2, "%.1fC", medidas_corrigidas.temp_aht20);  // Converte o inteiro em string
        sprintf(str_umi, "%.1f%%", medidas_corrigidas.hum_aht20);  // Converte o inteiro em string        

        // --- ALERTAS ---
        bool alerta = false;
        // Checa limites de temperatura (AHT20 e BMP280)
        if (medidas_corrigidas.temp_aht20 < min_temp_limit || medidas_corrigidas.temp_aht20 > max_temp_limit ||
            medidas_corrigidas.temp_bmp280 < min_temp_limit || medidas_corrigidas.temp_bmp280 > max_temp_limit) {
            alerta = true;
        }
        // Checa limites de umidade
        if (medidas_corrigidas.hum_aht20 < min_hum_limit || medidas_corrigidas.hum_aht20 > max_hum_limit) {
            alerta = true;
        }
        // Checa limites de pressão
        if (medidas_corrigidas.press_bmp280 < min_press_limit || medidas_corrigidas.press_bmp280 > max_press_limit) {
            alerta = true;
        }

        // ALERTA VISUAL (LED RGB)
        if (alerta) {
            gpio_put(LED_PIN_RED, 1); // Liga LED vermelho
            gpio_put(LED_PIN_GREEN, 0);
            iniciar_buzzer(BUZZER_A); // Liga o buzzer
            display_desenho(1); // Atualiza a matriz de LEDs para vermelho
        } else {
            gpio_put(LED_PIN_RED, 0);
            gpio_put(LED_PIN_GREEN, 1); // Verde normal
            parar_buzzer(BUZZER_A); // Desliga o buzzer
            display_desenho(0); // Atualiza a matriz de LEDs para verde
        }

        //  Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor);                           // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);       // Desenha um retângulo
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);            // Desenha uma linha
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);            // Desenha uma linha
        ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);  // Desenha uma string
        ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);   // Desenha uma string
        ssd1306_draw_string(&ssd, "BMP280  AHT10", 10, 28); // Desenha uma string
        ssd1306_line(&ssd, 63, 25, 63, 60, cor);            // Desenha uma linha vertical
        ssd1306_draw_string(&ssd, str_tmp1, 14, 41);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_press, 7, 52);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_tmp2, 73, 41);             // Desenha uma string
        ssd1306_draw_string(&ssd, str_umi, 73, 52);            // Desenha uma string
        ssd1306_send_data(&ssd);                            // Atualiza o display

        sleep_ms(500);
        // Mantém o servidor HTTP ativo
        cyw43_arch_poll();
    }
     cyw43_arch_deinit();// Esperamos que nunca chegue aqui

    return 0;
}


// Função de callback para enviar dados HTTP
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len)
    {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

// Função de recebimento HTTP
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{   

    if (!p)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs)
    {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0;

    
    if (strstr(req, "GET /estado")) {
        // Usa os valores já corrigidos
        char json_payload[512];
        int json_len = snprintf(json_payload, sizeof(json_payload),
            "{"
                "\"aht10_temp\":%.2f,"
                "\"aht10_humidity\":%.2f,"
                "\"bmp280_temp\":%.2f,"
                "\"bmp280_pressure\":%.2f,"
                "\"temp_min\":%.2f,"
                "\"temp_max\":%.2f,"
                "\"temp_offset\":%.2f,"
                "\"humidity_min\":%.2f,"
                "\"humidity_max\":%.2f,"
                "\"humidity_offset\":%.2f,"
                "\"pressure_min\":%.2f,"
                "\"pressure_max\":%.2f,"
                "\"pressure_offset\":%.2f"
            "}\r\n",
            medidas_corrigidas.temp_aht20, medidas_corrigidas.hum_aht20, medidas_corrigidas.temp_bmp280, medidas_corrigidas.press_bmp280,
            min_temp_limit, max_temp_limit, temp_offset,
            min_hum_limit, max_hum_limit, hum_offset,
            min_press_limit, max_press_limit, press_offset
        );
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            json_len, json_payload);
    }
    else if (strstr(req, "POST /limites")) {
        char *body = strstr(req, "\r\n\r\n");
        if(body) {
            body += 4;
            // Espera JSON no formato:
            // {"temp_min":10,"temp_max":35,"temp_offset":0.5,"humidity_min":20,"humidity_max":80,"humidity_offset":2,"pressure_min":950,"pressure_max":1050,"pressure_offset":0}
            float temp_min, temp_max, temp_off;
            float humidity_min, humidity_max, humidity_off;
            float pressure_min, pressure_max, pressure_off;
            temp_min = min_temp_limit;
            temp_max = max_temp_limit;
            temp_off = temp_offset;
            humidity_min = min_hum_limit;
            humidity_max = max_hum_limit;
            humidity_off = hum_offset;
            pressure_min = min_press_limit;
            pressure_max = max_press_limit;
            pressure_off = press_offset;
            sscanf(body,
                "{\"temp_min\":%f,\"temp_max\":%f,\"temp_offset\":%f,\"humidity_min\":%f,\"humidity_max\":%f,\"humidity_offset\":%f,\"pressure_min\":%f,\"pressure_max\":%f,\"pressure_offset\":%f",
                &temp_min, &temp_max, &temp_off,
                &humidity_min, &humidity_max, &humidity_off,
                &pressure_min, &pressure_max, &pressure_off);
            // Validação simples
            if (temp_max > temp_min) {
                max_temp_limit = temp_max;
                min_temp_limit = temp_min;
            }
            if (humidity_max > humidity_min) {
                max_hum_limit = humidity_max;
                min_hum_limit = humidity_min;
            }
            if (pressure_max > pressure_min) {
                max_press_limit = pressure_max;
                min_press_limit = pressure_min;
            }
            temp_offset = temp_off;
            hum_offset = humidity_off;
            press_offset = pressure_off;
        }
        const char *txt = "Limites e offsets atualizados";
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            (int)strlen(txt), txt);
    }
    else{
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           (int)strlen(html_data), html_data);
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);

    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    return ERR_OK;
}

// Função de callback para aceitar conexões TCP
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

// Função para iniciar o servidor HTTP
static void start_http_server(void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}

// Rotina para definição da intensidade de cores do led
uint32_t matrix_rgb(unsigned r, unsigned g, unsigned b){
    return (g << 24) | (r << 16) | (b << 8);
}

// Rotina para desenhar o padrão de LED
void display_desenho(int number){
    uint32_t valor_led;

    for (int i = 0; i < LED_COUNT; i++){
        // Define a cor do LED de acordo com o padrão
        if (padrao_led[number][ordem[24 - i]] == 1){
            valor_led = matrix_rgb(0, 10, 00); // Verde
        } else if (padrao_led[number][ordem[24 - i]] == 2){
            valor_led = matrix_rgb(10, 0, 0); // Vermelho
        } else{
            valor_led = matrix_rgb(0, 0, 0); // Desliga o LED
        }
        // Atualiza o LED
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}

// Configuração do PWM
void pwm_setup(uint pino) {
    gpio_set_function(pino, GPIO_FUNC_PWM);   // Configura o pino como saída PWM
    uint slice = pwm_gpio_to_slice_num(pino); // Obtém o slice correspondente
    
    pwm_set_wrap(slice, max_value_joy);  // Define o valor máximo do PWM

    pwm_set_enabled(slice, true);  // Habilita o slice PWM
}

// Função para iniciar o buzzer
void iniciar_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin); // Obtém o slice correspondente

    pwm_set_clkdiv(slice_num, 125); // Define o divisor de clock
    pwm_set_wrap(slice_num, 1000);  // Define o valor máximo do PWM

    pwm_set_gpio_level(pin, 10); //Para um som mais baixo foi colocado em 10
    pwm_set_enabled(slice_num, true);
}

// Função para parar o buzzer
void parar_buzzer(uint pin) {
    uint slice_num = pwm_gpio_to_slice_num(pin); // Obtém o slice correspondente
    pwm_set_enabled(slice_num, false); // Desabilita o slice PWM
    gpio_put(pin, 0); // Coloca o pino em nível para garantir que o buzzer está desligado
}