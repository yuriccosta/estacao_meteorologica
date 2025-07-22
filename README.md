# Estação Meteorológica IoT com Interface Web Responsiva e Alertas Inteligentes

Este projeto implementa uma estação meteorológica embarcada, utilizando a plataforma **BitDogLab com Raspberry Pi Pico W**, para monitorar temperatura, umidade e pressão barométrica em tempo real. Os dados são exibidos em uma página web dinâmica e em um display OLED local, e o sistema conta com alertas visuais e sonoros, além de permitir ajustes de calibração e limites via interface web ou botão físico.

---

## Objetivo

O objetivo do projeto é implementar uma estação meteorológica embarcada que monitore temperatura, umidade e pressão barométrica em tempo real. O sistema apresenta os dados em uma página web dinâmica e em um display OLED local, além de oferecer alertas visuais e sonoros e permitir ajustes de calibração e limites.

---

## Funcionalidades

* **Leitura de Sensores I2C**: Medição de temperatura e umidade com o sensor AHT10, e pressão atmosférica e temperatura com o sensor BMP280.
* **Interface Web Responsiva**: Exibição dos dados em tempo real com atualizações automáticas a cada 5 segundos via AJAX (JSON). A interface foi desenvolvida com HTML/CSS/JavaScript e a biblioteca ApexCharts para os gráficos.
* **Configuração Remota**: A interface web possui um formulário para configurar os limites mínimo e máximo de cada variável e valores de offset para calibração dos sensores.
* **Display OLED Local**: Exibe inicialmente o status da conexão Wi-Fi e o IP, e depois mostra os valores de temperatura, umidade e pressão de forma cíclica.
* **Alertas Visuais e Sonoros**: O LED RGB acende em vermelho para indicar que os limites foram ultrapassados e em verde para indicar normalidade. A matriz de LEDs exibe um ícone de losango ou "X", e um buzzer emite um sinal sonoro em caso de alerta.
* **Botão Físico com Debounce**: Um botão físico (Botão B) com tratamento de interrupção e debounce permite resetar os valores de limite para os padrões predefinidos.

---

## Componentes Utilizados

| Componente          | Função no Sistema                                                              |
| ------------------- | ------------------------------------------------------------------------------ |
| Sensor AHT10 (I2C)  | Mede a temperatura e a umidade relativa do ar.                                 |
| Sensor BMP280 (I2C) | Realiza a medição da pressão atmosférica e da temperatura.                     |
| Display OLED SSD1306| Exibe o status da conexão, o endereço IP e os dados dos sensores localmente.    |
| LED RGB             | Indica o estado geral do sistema (Verde: normal; Vermelho: alerta).            |
| Matriz de LEDs      | Exibe um ícone visual correspondente ao status da estação (ok ou fora do limite). |
| Buzzer              | Emite alertas sonoros quando os dados ultrapassam os limites definidos.        |
| Botão B (GPIO)      | Reseta os limites de alerta para os valores padrão.                            |
| Wi-Fi (Pico W)      | Hospeda a interface web e transmite os dados dos sensores.                     |

---

## Cores do LED RGB

* **Verde**: Condições normais, dentro dos limites configurados.
* **Vermelho**: Alerta, indica que uma ou mais medições estão fora dos limites definidos.

---

## Estrutura do Código

* **Leitura dos Sensores**: O código realiza a leitura dos sensores AHT10 e BMP280 via comunicação I2C.
* **Servidor Web Embarcado**: A Raspberry Pi Pico W hospeda uma página web para visualização e controle.
* **Comunicação AJAX**: A página web utiliza AJAX para solicitar e receber os dados dos sensores em formato JSON a cada 5 segundos, permitindo a atualização dinâmica dos gráficos.
* **Gerenciamento de Alertas**: O sistema compara as leituras atuais com os limites definidos pelo usuário para ativar os alertas visuais (LED RGB, Matriz de LEDs) e sonoro (Buzzer).
* **Exibição no Display**: O display OLED é atualizado ciclicamente para mostrar as informações mais recentes de telemetria e status da rede.
* **Controle por Botão Físico**: Utiliza uma interrupção com lógica de debounce para o botão que reseta as configurações de alerta.

---
