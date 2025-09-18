#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"


// ------------------------- DISPLAY -------------------------
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
struct render_area frame_area = {
    start_column : 0,
    end_column : ssd1306_width - 1,
    start_page : 0,
    end_page : ssd1306_n_pages - 1
};

// Limpar o display
void clean_display(uint8_t ssd[ssd1306_buffer_length])
{
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);
}

void write_display(char *text[], size_t num_lines, uint8_t ssd[ssd1306_buffer_length])
{
    int y = 0;
    // Escreve no display
    for (uint i = 0; i < num_lines; i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, &frame_area);
}


// ------------------------- BUZZER -------------------------
#define BUZZER_PIN 21

// Notas musicais para a música tema de Star Wars
const uint star_wars_notes[] = {784, 880, 988, 880, 784};

// Duração das notas em milissegundos
const uint note_duration[] = {150, 150, 200, 150, 300};

// Inicializa o PWM no pino do buzzer
void pwm_init_buzzer(uint pin)
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f); // Ajusta divisor de clock
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0); // Desliga o PWM inicialmente
}

// Toca uma nota com a frequência e duração especificadas
void play_tone(uint pin, uint frequency, uint duration_ms)
{
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1;

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(pin, top / 2); // 50% de duty cycle

    sleep_ms(duration_ms);

    pwm_set_gpio_level(pin, 0); // Desliga o som após a duração
    sleep_ms(50);               // Pausa entre notas
}

// Função principal para tocar a música
void play_star_wars(uint pin)
{
    for (int i = 0; i < sizeof(star_wars_notes) / sizeof(star_wars_notes[0]); i++)
    {
        if (star_wars_notes[i] == 0)
        {
            sleep_ms(note_duration[i]);
        }
        else
        {
            play_tone(pin, star_wars_notes[i], note_duration[i]);
        }
    }
}


// ------------------------- RPM -------------------------
#define SENSOR_PIN 16
#define PULSES_PER_REV 1  // ajuste conforme a quantidade de marcas no disco

volatile uint32_t pulse_count = 0;

// Callback da interrupção do GPIO
void gpio_callback(uint gpio, uint32_t events) {
    pulse_count++;
}

// Função para calcular RPM
float calculate_rpm(uint32_t pulses, uint64_t elapsed_ms) {
    if (elapsed_ms == 0) return 0.0f;
    float revolutions = (float)pulses / (float)PULSES_PER_REV;
    return (revolutions * 60000.0f) / (float)elapsed_ms;
}



int main()
{
    stdio_init_all();

    // DISPLAY

    // I2C1: usado para o display SSD1306
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicialização do display
    ssd1306_init();
    calculate_render_area_buffer_length(&frame_area);
    uint8_t ssd[ssd1306_buffer_length];
    clean_display(ssd);

    char *text[1];

    text[0] = "Hello, Word!";

    write_display(text, 1, ssd);

    // BUZZER
    pwm_init_buzzer(BUZZER_PIN);

    // RPM
    // Configuração do pino do sensor
    gpio_init(SENSOR_PIN);
    gpio_set_dir(SENSOR_PIN, GPIO_IN);
    gpio_pull_down(SENSOR_PIN); // Mantém em LOW quando sem sinal

     // Habilitar interrupção nas duas bordas (subida e descida)
    gpio_set_irq_enabled_with_callback(
        SENSOR_PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true,
        &gpio_callback
    );

    uint64_t last_time = to_ms_since_boot(get_absolute_time());

    printf("Contagem de RPM iniciada...\n");

    while (true)
    {
        // play_star_wars(BUZZER_PIN);

        uint64_t now = to_ms_since_boot(get_absolute_time());
        uint64_t elapsed = now - last_time;
        last_time = now;

        // Captura e zera o contador de pulsos
        uint32_t pulses = pulse_count;
        pulse_count = 0;

        // Calcula RPM
        float rpm = calculate_rpm(pulses, elapsed);

        printf("Pulsos: %u | Tempo: %llu ms | RPM: %.2f\n", pulses, elapsed, rpm);

        if ( rpm > 0 && rpm < 400) {
            play_star_wars(BUZZER_PIN);
            
            clean_display(ssd);
            text[0] = "    Alerta!    ";
            write_display(text, 1, ssd);
            clean_display(ssd);
        }

        sleep_ms(1000); // atualiza a cada 1 segundo
    }

    return 0;
}
