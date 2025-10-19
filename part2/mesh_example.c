// ATWINC1500 Mesh Networking Example for Pico 2W
//
// This example demonstrates how to use the ATWINC1500 WiFi module
// in mesh networking mode on the Raspberry Pi Pico 2W (RP2350)
//
// Modified by Niles Roxas (2025)
// Based on original work by Jeremy P Bentham

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "winc_wifi.h"
#include "winc_sock.h"
#include "winc_p2p.h"

#define VERBOSE     2           // Diagnostic output level (0 to 3)
#define SPI_SPEED   11000000    // SPI clock (actually 10.42 MHz)
#define SPI_PORT    spi0        // SPI port number

// Pin definitions for Pico 2W with ATWINC1500
#define SCK_PIN     18
#define MOSI_PIN    19
#define MISO_PIN    16
#define CS_PIN      17
#define WAKE_PIN    20
#define RESET_PIN   21
#define IRQ_PIN     22

// Mesh configuration
// NOTE: Change these values for each node in your mesh network
#define MESH_NODE_ID     1
#define MESH_NODE_NAME   "PicoNode1"
#define P2P_CHAN         1

#define MESH_UDP_PORT    1025
#define MESH_TCP_PORT    1026

extern int verbose;

// Return microsecond time
uint32_t usec(void)
{
    return(time_us_32());
}

// Do SPI transfer
int spi_xfer(int fd, uint8_t *txd, uint8_t *rxd, int len)
{
    if (verbose > 2)
    {
        printf("  Tx:");
        for (int i=0; i<len; i++)
            printf(" %02X", txd[i]);
    }
    gpio_put(CS_PIN, 0);
    spi_write_read_blocking(SPI_PORT, txd, rxd, len);
    while (gpio_get(SCK_PIN)) ;
    gpio_put(CS_PIN, 1);
    if (verbose > 2)
    {
        printf("\n  Rx:");
        for (int i=0; i<len; i++)
            printf(" %02X", rxd[i]);
        printf("\n");
    }
    return(len);
}

// Read IRQ line
int read_irq(void)
{
    return(gpio_get(IRQ_PIN));
}

// Initialise SPI interface
void spi_setup(int fd)
{
    stdio_init_all();
    spi_init(SPI_PORT, SPI_SPEED);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_init(MISO_PIN);
    gpio_set_function(MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(CS_PIN,   GPIO_FUNC_SIO);
    gpio_set_function(SCK_PIN,  GPIO_FUNC_SPI);
    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
    gpio_init(CS_PIN);
    gpio_set_dir(CS_PIN, GPIO_OUT);
    gpio_put(CS_PIN, 1);
    gpio_init(WAKE_PIN);
    gpio_set_dir(WAKE_PIN, GPIO_OUT);
    gpio_put(WAKE_PIN, 1);
    gpio_init(IRQ_PIN);
    gpio_set_dir(IRQ_PIN, GPIO_IN);
    gpio_pull_up(IRQ_PIN);
    gpio_init(RESET_PIN);
    gpio_set_dir(RESET_PIN, GPIO_OUT);
    gpio_put(RESET_PIN, 0);
    sleep_ms(1);
    gpio_put(RESET_PIN, 1);
    sleep_ms(1);
}

// Custom mesh data handler
void mesh_data_rx_handler(int fd, uint8_t sock, int rxlen)
{
    uint8_t data[256];

    printf("\n=== Mesh Data Received ===\n");
    printf("Socket: %u, Length: %d\n", sock, rxlen);

    if (rxlen > 0 && rxlen <= sizeof(data))
    {
        if (get_sock_data(fd, sock, data, rxlen))
        {
            printf("Data: ");
            for (int i = 0; i < rxlen; i++)
            {
                if (data[i] >= 32 && data[i] < 127)
                    putchar(data[i]);
                else
                    printf("<%02X>", data[i]);
            }
            printf("\n");
            printf("========================\n\n");

            // Echo the data back
            put_sock_sendto(fd, sock, data, rxlen);
        }
    }
}

int main(void)
{
    int fd;
    bool ok;
    int sock_udp, sock_tcp;
    uint32_t last_beacon = 0;
    uint32_t last_status = 0;

    verbose = VERBOSE;

    printf("\n");
    printf("=====================================\n");
    printf("  ATWINC1500 Mesh Network Example\n");
    printf("  Raspberry Pi Pico 2W (RP2350)\n");
    printf("=====================================\n\n");

    printf("Node Configuration:\n");
    printf("  ID: %u\n", MESH_NODE_ID);
    printf("  Name: %s\n", MESH_NODE_NAME);
    printf("  P2P Channel: %u\n", P2P_CHAN);
    printf("\n");

    // Initialize SPI and ATWINC1500
    printf("Initializing SPI interface...\n");
    spi_setup(fd);

    printf("Disabling CRC...\n");
    disable_crc(fd);

    printf("Initializing ATWINC1500 chip...\n");
    ok = chip_init(fd);

    if (!ok)
    {
        printf("ERROR: Failed to initialize chip!\n");
        return -1;
    }

    printf("Getting chip information...\n");
    ok = chip_get_info(fd);

    if (!ok)
    {
        printf("ERROR: Failed to get chip info!\n");
        return -1;
    }

    // Configure GPIO
    ok = ok && set_gpio_val(fd, 0x58070) && set_gpio_dir(fd, 0x58070);

    if (!ok)
    {
        printf("ERROR: Failed to configure GPIO!\n");
        return -1;
    }

    printf("\n=== Starting Mesh Network ===\n\n");

    // Enable P2P mode
    printf("Enabling P2P mode on channel %u...\n", P2P_CHAN);
    ok = p2p_enable(fd, P2P_CHAN);

    if (!ok)
    {
        printf("ERROR: Failed to enable P2P mode!\n");
        return -1;
    }

    // Initialize and enable mesh networking
    printf("Initializing mesh network...\n");
    ok = mesh_init(fd, MESH_NODE_ID, MESH_NODE_NAME);

    if (!ok)
    {
        printf("ERROR: Failed to initialize mesh!\n");
        return -1;
    }

    printf("Enabling mesh networking...\n");
    ok = mesh_enable(fd);

    if (!ok)
    {
        printf("ERROR: Failed to enable mesh networking!\n");
        return -1;
    }

    // Set up sockets for mesh communication
    printf("Setting up mesh communication sockets...\n");

    sock_udp = open_sock_server(MESH_UDP_PORT, 0, mesh_data_rx_handler);
    if (sock_udp < 0)
    {
        printf("ERROR: Failed to create UDP socket!\n");
        return -1;
    }
    printf("  UDP socket %d on port %u OK\n", sock_udp, MESH_UDP_PORT);

    sock_tcp = open_sock_server(MESH_TCP_PORT, 1, mesh_data_rx_handler);
    if (sock_tcp < 0)
    {
        printf("ERROR: Failed to create TCP socket!\n");
        return -1;
    }
    printf("  TCP socket %d on port %u OK\n", sock_tcp, MESH_TCP_PORT);

    printf("\n=== Mesh Network Active ===\n");
    printf("Listening for P2P connections...\n");
    printf("Sending periodic beacons...\n");
    printf("Commands:\n");
    printf("  [Will auto-print status every 30s]\n");
    printf("\n");

    // Main loop
    uint32_t loop_count = 0;
    while (true)
    {
        uint32_t now = time_us_32() / 1000; // Convert to milliseconds

        // Handle interrupts from ATWINC1500
        if (read_irq() == 0)
        {
            interrupt_handler();
        }

        // Send mesh beacons periodically
        if (now - last_beacon > MESH_BEACON_INTERVAL)
        {
            mesh_send_beacon(fd);
            last_beacon = now;
        }

        // Handle mesh beacon processing
        mesh_beacon_handler(fd);

        // Print status every 30 seconds
        if (now - last_status > 30000)
        {
            printf("\n--- Status Update (Loop: %lu) ---\n", loop_count);
            mesh_print_routing_table();
            printf("P2P Mode: %s\n", is_p2p_enabled() ? "Enabled" : "Disabled");
            printf("Mesh Mode: %s\n", is_mesh_enabled() ? "Enabled" : "Disabled");
            printf("--------------------------------\n\n");
            last_status = now;
        }

        loop_count++;

        // Small delay to prevent busy-waiting
        sleep_ms(10);
    }

    return 0;
}

// EOF
