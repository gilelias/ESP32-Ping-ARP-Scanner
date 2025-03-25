#include <WiFi.h>
#include <ESP32Ping.h>
#include "lwip/etharp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Network Credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Mutex for linked list access
SemaphoreHandle_t listMutex;


// Number of scanning tasks
#define NUM_TASKS 4

netif* interface;

struct DeviceNode {
    IPAddress ip;
    uint8_t mac[6];
    DeviceNode* next;
};

DeviceNode* head = nullptr;

// Push a new device to the linked list
void pushDevice(IPAddress ip, uint8_t mac[6]) {
    xSemaphoreTake(listMutex, portMAX_DELAY);
    DeviceNode* newNode = new DeviceNode;
    newNode->ip = ip;
    memcpy(newNode->mac, mac, 6);
    newNode->next = head;
    head = newNode;
    xSemaphoreGive(listMutex);
}

// Clear the linked list
void clearDeviceList() {
    xSemaphoreTake(listMutex, portMAX_DELAY);
    while (head) {
        DeviceNode* temp = head;
        head = head->next;
        delete temp;
    }
    xSemaphoreGive(listMutex);
}

// Print all discovered devices
void printDevices() {
    xSemaphoreTake(listMutex, portMAX_DELAY);
    DeviceNode* current = head;
    while (current) {
        Serial.printf("Device IP: %s, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      current->ip.toString().c_str(),
                      current->mac[0], current->mac[1], current->mac[2],
                      current->mac[3], current->mac[4], current->mac[5]);
        current = current->next;
    }
    xSemaphoreGive(listMutex);
}

// Send an ARP request to get the MAC address
void sendArp(IPAddress remote_ip) {
    ip4_addr origin;
    origin.addr = remote_ip;
    if (interface->hwaddr_len == 6) {
        etharp_request(interface, &origin);
    }
}

// Retrieve MAC address from ARP cache
bool getArpResult(IPAddress remote_ip, uint8_t* mac) {
    ip4_addr origin;
    origin.addr = remote_ip;
    eth_addr* ethadress;
    const ip4_addr* ipaddress;

    if (interface->hwaddr_len == 6) {
        err_t result = etharp_find_addr(interface, &origin, &ethadress, &ipaddress);
        if (result > -1 && ethadress) {
            memcpy(mac, ethadress->addr, 6);
            return true;
        }
    }
    return false;
}

// Get the total number of usable IPs in the subnet
uint32_t getSubnetSize(IPAddress subnetMask) {
    uint32_t mask = (subnetMask[0] << 24) | (subnetMask[1] << 16) | (subnetMask[2] << 8) | subnetMask[3];
    uint32_t hostCount = ~mask;  // Number of hosts in subnet
    return hostCount - 1;  // Exclude network and broadcast addresses
}

// Scan a portion of the network
void scanNetworkSegment(uint32_t startIP, uint32_t endIP) {
    for (uint32_t ip = startIP; ip <= endIP; ip++) {
        IPAddress targetIP((ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);

        if (targetIP == WiFi.localIP()) continue;  // Skip local device

        if (Ping.ping(targetIP, 1)) {
            uint8_t mac[6] = {0};
            sendArp(targetIP);
            vTaskDelay(pdMS_TO_TICKS(50));  // Short delay for ARP response

            if (getArpResult(targetIP, mac)) {
                pushDevice(targetIP, mac);
                Serial.printf("Device Found: %s\n", targetIP.toString().c_str());
            }
        }
    }
}

// Task to scan a specific portion of the network
void scanTask(void* parameter) {
    uint32_t* range = (uint32_t*)parameter;
    scanNetworkSegment(range[0], range[1]);
    delete[] range;  // Free allocated memory
    vTaskDelete(NULL);
}


void networkScannerPrint(void* parameter){
    while (true) {
        printDevices();
        vTaskDelay(pdMS_TO_TICKS(30000));  // Wait 30 seconds before printing again
    }
}


// Start parallel network scan
void startParallelScan() {
    clearDeviceList();
    IPAddress subnetMask = WiFi.subnetMask();
    IPAddress localIP = WiFi.localIP();

    uint32_t baseIP = (localIP[0] << 24) | (localIP[1] << 16) | (localIP[2] << 8);
    uint32_t subnetSize = getSubnetSize(subnetMask);

    if (subnetSize < 2) {
        Serial.println("Invalid subnet size detected!");
        return;
    }

    uint32_t startIP = baseIP + 1;
    uint32_t endIP = baseIP + subnetSize - 1;

    Serial.printf("Scanning subnet: %s/%d\n", localIP.toString().c_str(), subnetMask[3]);
    Serial.printf("Total Usable IPs: %d, Task Split: %d IPs per task\n", subnetSize, subnetSize / NUM_TASKS);

    // Spawn tasks for parallel scanning
    uint32_t rangeSize = subnetSize / NUM_TASKS;
    for (int i = 0; i < NUM_TASKS; i++) {
        uint32_t* range = new uint32_t[2];
        range[0] = startIP + i * rangeSize;
        range[1] = (i == NUM_TASKS - 1) ? endIP : (range[0] + rangeSize - 1);
        xTaskCreate(scanTask, "ScanTask", 4096, range, 1, NULL);
    }
}

// Background scanning task
void networkScannerTask(void* parameter) {
    while (true) {
        startParallelScan();
        printDevices();
        vTaskDelay(pdMS_TO_TICKS(600000));  // Wait 10 minutes before scanning again
    }
}

// Setup function
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    interface = netif_list;
    while (interface && interface->hwaddr_len != 6) {
        interface = interface->next;
    }
    Serial.printf("Interface is %d\n", interface->hwaddr_len);

    listMutex = xSemaphoreCreateMutex();
    if (listMutex == NULL) {
        Serial.println("Error: Mutex creation failed!");
    }

    xTaskCreate(networkScannerTask, "NetworkScanner", 8192, NULL, 1, NULL);
    xTaskCreate(networkScannerPrint, "NetworkPrinter", 8192, NULL, 1, NULL);
}

// Empty loop since FreeRTOS handles execution
void loop() {
    vTaskDelay(portMAX_DELAY);
}
