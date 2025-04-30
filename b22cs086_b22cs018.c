#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// BFT Simulation Constants
#define NUM_NODES 4                // Total number of nodes in system
#define FAULTY_NODE 1              // Node that may exhibit Byzantine behavior
#define MAX_ROUNDS 5               // Number of consensus rounds to run
#define FAULTY_PROBABILITY 30      // Probability (%) of faulty node misbehaving
#define MAX_MESSAGE_SIZE 100       // Maximum size of a message
#define BUFFER_SIZE 10             // Size of each circular message buffer
#define MAX_TIMEOUT 50             // Maximum cycles a node can stay in a state

// Message types
typedef enum {
    MSG_PROPOSE,
    MSG_PREVOTE,
    MSG_PRECOMMIT,
    MSG_COMMIT
} MessageType;

// Node states
typedef enum {
    STATE_INITIAL,
    STATE_PROPOSE,
    STATE_PREVOTE,
    STATE_PRECOMMIT,
    STATE_COMMIT,
    STATE_DECIDED
} NodeState;

// Message structure
typedef struct {
    int sender;
    MessageType type;
    int round;
    int value;
    bool valid;
} Message;

// Message buffer structure (circular buffer)
typedef struct {
    Message messages[BUFFER_SIZE];
    int read_index;
    int write_index;
    int count;
} MessageBuffer;

// Node structure
typedef struct {
    int id;
    bool is_faulty;
    NodeState state;
    int current_round;
    int proposed_value;
    int committed_value;
    int prevote_count[2];   // Count of prevotes for values 0 and 1
    int precommit_count[2]; // Count of precommits for values 0 and 1
    int commit_count[2];    // Count of commits for values 0 and 1
    int timeout_counter;    // Counter for state timeouts
    char status[MAX_MESSAGE_SIZE]; // Current status message
} Node;

// Global variables
static Node nodes[NUM_NODES];
static MessageBuffer message_buffers[NUM_NODES][NUM_NODES]; // [sender][receiver]
static uint32_t random_seed = 0;

// Function prototypes
void SystemClock_Config(void);
void UART1_Init(void);
void UART1_SendChar(char c);
void UART1_SendString(char *str);
void delay_ms(uint32_t ms);
char* itoa_simple(int num, char* str);
int get_random_value(int max_value);
void initialize_nodes(void);
void initialize_message_buffers(void);
bool buffer_is_empty(MessageBuffer* buffer);
bool buffer_is_full(MessageBuffer* buffer);
bool send_message(int sender, int receiver, MessageType type, int round, int value, bool valid);
bool receive_message(int receiver, Message* msg);
bool process_message(int node_id, Message* msg);
bool run_node_step(int node_id);
void print_buffer_status(void);
const char* state_to_string(NodeState state);
const char* message_type_to_string(MessageType type);
void output_status(int node_id, const char* message);

int main(void) {
    // Configure system clock
    SystemClock_Config();
    
    // Initialize UART
    UART1_Init();
    
    // Seed random number generator
    random_seed = SysTick->VAL;
    
    // Print welcome message
    UART1_SendString("\r\n\r\n");
    UART1_SendString("**************************************************\r\n");
    UART1_SendString("* Byzantine Fault Tolerance Simulation on STM32  *\r\n");
    UART1_SendString("**************************************************\r\n\r\n");
    
    char info[100];
    sprintf(info, "Nodes: %d, Faulty Node: %d, Rounds: %d\r\n", NUM_NODES, FAULTY_NODE, MAX_ROUNDS);
    UART1_SendString(info);
    UART1_SendString("Starting circular buffer message passing simulation...\r\n\r\n");
    
    // Initialize nodes and message buffers
    initialize_nodes();
    initialize_message_buffers();
    
    // Run simulation until all nodes complete all rounds
    bool simulation_complete = false;
    int debug_counter = 0;
    
    while (!simulation_complete) {
        simulation_complete = true;
        
        // Process one step for each node
        for (int i = 0; i < NUM_NODES; i++) {
            // Check if this node still has rounds to complete
            if (nodes[i].current_round < MAX_ROUNDS) {
                simulation_complete = false;
                
                // Process incoming messages (up to 3 per cycle)
                int processed_messages = 0;
                while (processed_messages < 3) {
                    Message msg;
                    if (receive_message(i, &msg)) {
                        process_message(i, &msg);
                        processed_messages++;
                    } else {
                        break; // No more messages to process
                    }
                }
                
                // Run one step of node's state machine
                bool state_changed = run_node_step(i);
                
                // Update timeout counter
                if (!state_changed) {
                    nodes[i].timeout_counter++;
                    
                    // Force progress after timeout
                    if (nodes[i].timeout_counter > MAX_TIMEOUT) {
                        char timeout_msg[100];
                        sprintf(timeout_msg, "TIMEOUT in state %s, forcing progress", 
                                state_to_string(nodes[i].state));
                        output_status(i, timeout_msg);
                        
                        // Force state transition based on current state
                        switch (nodes[i].state) {
                            case STATE_PREVOTE:
                                // If stuck in PREVOTE, move to PRECOMMIT with majority value
                                if (nodes[i].prevote_count[1] > nodes[i].prevote_count[0]) {
                                    // Prevote for 1
                                    for (int j = 0; j < NUM_NODES; j++) {
                                        send_message(i, j, MSG_PREVOTE, nodes[i].current_round, 1, true);
                                    }
                                } else {
                                    // Prevote for 0
                                    for (int j = 0; j < NUM_NODES; j++) {
                                        send_message(i, j, MSG_PREVOTE, nodes[i].current_round, 0, true);
                                    }
                                }
                                nodes[i].state = STATE_PRECOMMIT;
                                break;
                                
                            case STATE_PRECOMMIT:
                                // If stuck in PRECOMMIT, move to COMMIT with default value 0
                                for (int j = 0; j < NUM_NODES; j++) {
                                    send_message(i, j, MSG_PRECOMMIT, nodes[i].current_round, 0, true);
                                }
                                nodes[i].state = STATE_COMMIT;
                                break;
                                
                            case STATE_COMMIT:
                                // If stuck in COMMIT, move to DECIDED with default value 0
                                nodes[i].committed_value = 0;
                                for (int j = 0; j < NUM_NODES; j++) {
                                    send_message(i, j, MSG_COMMIT, nodes[i].current_round, 0, true);
                                }
                                nodes[i].state = STATE_DECIDED;
                                break;
                                
                            case STATE_DECIDED:
                                // If stuck in DECIDED, move to next round
                                sprintf(timeout_msg, "Round %d complete (timeout). Final value: %d",
                                        nodes[i].current_round, nodes[i].committed_value);
                                output_status(i, timeout_msg);
                                
                                // Reset for next round
                                nodes[i].current_round++;
                                nodes[i].state = STATE_INITIAL;
                                nodes[i].proposed_value = (nodes[i].committed_value != -1) ? 
                                                           nodes[i].committed_value : (i % 2);
                                
                                // Reset counters
                                nodes[i].prevote_count[0] = 0;
                                nodes[i].prevote_count[1] = 0;
                                nodes[i].precommit_count[0] = 0;
                                nodes[i].precommit_count[1] = 0;
                                nodes[i].commit_count[0] = 0;
                                nodes[i].commit_count[1] = 0;
                                break;
                                
                            default:
                                break;
                        }
                        
                        nodes[i].timeout_counter = 0;
                    }
                } else {
                    // Reset timeout counter on state change
                    nodes[i].timeout_counter = 0;
                }
            }
        }
        
        // Print buffer status occasionally
        if (debug_counter++ % 100 == 0) {
            print_buffer_status();
        }
        
        // Short delay to make output readable
        delay_ms(20);
    }
    
    // Display final results
    UART1_SendString("\r\n**************************************************\r\n");
    UART1_SendString("* BFT Simulation Complete                       *\r\n");
    UART1_SendString("**************************************************\r\n\r\n");
    
    UART1_SendString("Final consensus results:\r\n");
    for (int i = 0; i < NUM_NODES; i++) {
        char result[100];
        sprintf(result, "Node %d (%s): Final committed value = %d\r\n", 
                i, nodes[i].is_faulty ? "Faulty" : "Honest", 
                nodes[i].committed_value);
        UART1_SendString(result);
    }
    
    // Main loop - do nothing after simulation completes
    while (1) {
        delay_ms(1000);
    }
}

// Print status of all message buffers
void print_buffer_status(void) {
    UART1_SendString("\r\n--- Buffer Status ---\r\n");
    for (int sender = 0; sender < NUM_NODES; sender++) {
        for (int receiver = 0; receiver < NUM_NODES; receiver++) {
            char buf[50];
            sprintf(buf, "  S%d->R%d: %d msgs\r\n", 
                   sender, receiver, message_buffers[sender][receiver].count);
            UART1_SendString(buf);
        }
    }
    
    // Also print node states
    UART1_SendString("\r\n--- Node States ---\r\n");
    for (int i = 0; i < NUM_NODES; i++) {
        char state_info[100];
        sprintf(state_info, "  Node %d: Round %d, State %s, Timeout %d\r\n", 
                i, nodes[i].current_round, state_to_string(nodes[i].state), nodes[i].timeout_counter);
        UART1_SendString(state_info);
    }
    UART1_SendString("\r\n");
}

void SystemClock_Config(void) {
    // Enable HSI clock
    RCC->CR |= RCC_CR_HSION;
    while(!(RCC->CR & RCC_CR_HSIRDY));
    
    // Set HSI as system clock source
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_HSI;
    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);
}

void UART1_Init(void) {
    // Enable clock for GPIOA and USART1
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;  // Enable GPIOA clock
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;  // Enable USART1 clock
    
    // Configure PA9 (USART1 TX) and PA10 (USART1 RX)
    // Clear previous settings
    GPIOA->MODER &= ~(GPIO_MODER_MODER9 | GPIO_MODER_MODER10);
    // Set pins to alternate function mode
    GPIOA->MODER |= (GPIO_MODER_MODER9_1 | GPIO_MODER_MODER10_1);
    
    // Set alternate function AF7 (USART1) for PA9 and PA10
    GPIOA->AFR[1] &= ~(0xF << 4 | 0xF << 8);  // Clear AF for PA9 and PA10
    GPIOA->AFR[1] |= (7 << 4) | (7 << 8);     // Set AF7 for PA9 and PA10
    
    // Configure USART1
    // Reset USART1 configuration
    USART1->CR1 = 0;
    USART1->CR2 = 0;
    USART1->CR3 = 0;
    
    // Configure baud rate to 9600 assuming 16MHz HSI clock
    // USARTDIV = 16000000/(16*9600) = 104.1667 ˜ 104 = 0x068
    USART1->BRR = 0x0683;
    
    // Enable transmitter
    USART1->CR1 |= USART_CR1_TE;
    
    // Enable USART1
    USART1->CR1 |= USART_CR1_UE;
    
    // Add a small delay to ensure USART is ready
    for(volatile uint32_t i = 0; i < 10000; i++);
}

void UART1_SendChar(char c) {
    // Wait until the transmit data register is empty
    while (!(USART1->SR & USART_SR_TXE));
    // Send the character
    USART1->DR = c;
}

void UART1_SendString(char *str) {
    while (*str) {
        UART1_SendChar(*str++);
    }
}

void delay_ms(uint32_t ms) {
    // Configure SysTick for 1ms intervals
    SysTick->LOAD = 16000 - 1;  // 16MHz / 1000 = 16000 cycles per ms
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;
    
    for (uint32_t i = 0; i < ms; i++) {
        // Wait until count flag is set
        while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
    }
    
    SysTick->CTRL = 0;  // Disable SysTick
}

// Simple itoa function since standard library might not be fully available
char* itoa_simple(int num, char* str) {
    int i = 0;
    bool is_negative = false;
    
    // Handle negative numbers
    if (num < 0) {
        is_negative = true;
        num = -num;
    }
    
    // Handle case when num is 0
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }
    
    // Process individual digits
    while (num != 0) {
        int rem = num % 10;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / 10;
    }
    
    // Add negative sign if needed
    if (is_negative)
        str[i++] = '-';
    
    str[i] = '\0'; // Null-terminate string
    
    // Reverse the string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
    
    return str;
}

// Get a random value between 0 and max_value-1
int get_random_value(int max_value) {
    // Use a simple LCG (Linear Congruential Generator)
    random_seed = (random_seed * 1103515245 + 12345) & 0x7FFFFFFF;
    return (int)(random_seed % max_value);
}

// Initialize all nodes
void initialize_nodes(void) {
    for (int i = 0; i < NUM_NODES; i++) {
        nodes[i].id = i;
        nodes[i].is_faulty = (i == FAULTY_NODE);
        nodes[i].state = STATE_INITIAL;
        nodes[i].current_round = 0;
        nodes[i].proposed_value = (i % 2); // Alternate between 0 and 1 for demo
        nodes[i].committed_value = -1;
        nodes[i].timeout_counter = 0;
        
        // Initialize message counters
        for (int j = 0; j < 2; j++) {
            nodes[i].prevote_count[j] = 0;
            nodes[i].precommit_count[j] = 0;
            nodes[i].commit_count[j] = 0;
        }
        
        nodes[i].status[0] = '\0';
    }
}

// Initialize all message buffers
void initialize_message_buffers(void) {
    for (int i = 0; i < NUM_NODES; i++) {
        for (int j = 0; j < NUM_NODES; j++) {
            message_buffers[i][j].read_index = 0;
            message_buffers[i][j].write_index = 0;
            message_buffers[i][j].count = 0;
        }
    }
}

// Check if message buffer is empty
bool buffer_is_empty(MessageBuffer* buffer) {
    return (buffer->count == 0);
}

// Check if message buffer is full
bool buffer_is_full(MessageBuffer* buffer) {
    return (buffer->count == BUFFER_SIZE);
}

// Send a message from one node to another (push to circular buffer)
bool send_message(int sender, int receiver, MessageType type, int round, int value, bool valid) {
    MessageBuffer* buffer = &message_buffers[sender][receiver];
    
    // Check if buffer is full
    if (buffer_is_full(buffer)) {
        char error[100];
        sprintf(error, "ERROR: Message buffer full, dropping message from Node %d to Node %d\r\n", 
                sender, receiver);
        UART1_SendString(error);
        return false;
    }
    
    // Add message to buffer
    buffer->messages[buffer->write_index].sender = sender;
    buffer->messages[buffer->write_index].type = type;
    buffer->messages[buffer->write_index].round = round;
    buffer->messages[buffer->write_index].value = value;
    buffer->messages[buffer->write_index].valid = valid;
    
    // Update write index and count
    buffer->write_index = (buffer->write_index + 1) % BUFFER_SIZE;
    buffer->count++;
    
    return true;
}

// Receive a message for a node (pop from circular buffer)
bool receive_message(int receiver, Message* msg) {
    // Check all sender buffers for messages to this receiver, one at a time
    static int last_sender = 0;  // Check different senders in round-robin fashion
    
    for (int s = 0; s < NUM_NODES; s++) {
        int sender = (last_sender + s) % NUM_NODES;  // Start from where we left off
        MessageBuffer* buffer = &message_buffers[sender][receiver];
        
        // Check if this buffer has any messages
        if (!buffer_is_empty(buffer)) {
            // Get message from buffer
            *msg = buffer->messages[buffer->read_index];
            
            // Update read index and count
            buffer->read_index = (buffer->read_index + 1) % BUFFER_SIZE;
            buffer->count--;
            
            // Update where to start checking next time
            last_sender = (sender + 1) % NUM_NODES;
            
            return true;
        }
    }
    
    // No messages found
    return false;
}

// Process a received message
bool process_message(int node_id, Message* msg) {
    // Only process messages for the current round
    if (msg->round != nodes[node_id].current_round) {
        return false;
    }
    
    // Update counters based on message type
    if (msg->valid) {
        switch (msg->type) {
            case MSG_PROPOSE:
                // No specific counter for propose messages
                break;
            case MSG_PREVOTE:
                if (msg->value == 0 || msg->value == 1) {
                    nodes[node_id].prevote_count[msg->value]++;
                }
                break;
            case MSG_PRECOMMIT:
                if (msg->value == 0 || msg->value == 1) {
                    nodes[node_id].precommit_count[msg->value]++;
                }
                break;
            case MSG_COMMIT:
                if (msg->value == 0 || msg->value == 1) {
                    nodes[node_id].commit_count[msg->value]++;
                }
                break;
        }
    }
    
    return true;
}

// Convert node state to string for display
const char* state_to_string(NodeState state) {
    switch (state) {
        case STATE_INITIAL:   return "INITIAL";
        case STATE_PROPOSE:   return "PROPOSE";
        case STATE_PREVOTE:   return "PREVOTE";
        case STATE_PRECOMMIT: return "PRECOMMIT";
        case STATE_COMMIT:    return "COMMIT";
        case STATE_DECIDED:   return "DECIDED";
        default:              return "UNKNOWN";
    }
}

// Convert message type to string for display
const char* message_type_to_string(MessageType type) {
    switch (type) {
        case MSG_PROPOSE:   return "PROPOSE";
        case MSG_PREVOTE:   return "PREVOTE";
        case MSG_PRECOMMIT: return "PRECOMMIT";
        case MSG_COMMIT:    return "COMMIT";
        default:            return "UNKNOWN";
    }
}

// Output node status (prints to UART)
void output_status(int node_id, const char* message) {
    char buffer[MAX_MESSAGE_SIZE];
    sprintf(buffer, "Node %d (%s): %s\r\n", 
            node_id, 
            nodes[node_id].is_faulty ? "Faulty" : "Honest",
            message);
    UART1_SendString(buffer);
}

// Run one step of a node's state machine logic
bool run_node_step(int node_id) {
    Node* node = &nodes[node_id];
    char status_msg[MAX_MESSAGE_SIZE];
    
    // State machine logic (one step at a time)
    switch (node->state) {
        case STATE_INITIAL:
            // Move to propose state
            node->state = STATE_PROPOSE;
            
            sprintf(status_msg, "Starting round %d in %s state. Proposing value: %d", 
                    node->current_round, state_to_string(node->state), node->proposed_value);
            output_status(node_id, status_msg);
            
            // Send propose messages to all nodes
            for (int i = 0; i < NUM_NODES; i++) {
                // If this is the faulty node, it might send different values
                bool be_faulty = node->is_faulty && (get_random_value(100) < FAULTY_PROBABILITY);
                int value_to_send = node->proposed_value;
                
                if (be_faulty) {
                    value_to_send = 1 - value_to_send; // Flip the value
                    sprintf(status_msg, "BYZANTINE BEHAVIOR: Sending incorrect value %d to node %d", 
                            value_to_send, i);
                    output_status(node_id, status_msg);
                }
                
                send_message(node_id, i, MSG_PROPOSE, node->current_round, value_to_send, true);
            }
            
            // Move to prevote state
            node->state = STATE_PREVOTE;
            return true;
            
        case STATE_PREVOTE: {
            // Check if we received enough propose messages
            int propose_count = 0;
            int votes[2] = {0, 0};
            
            // Count propose messages directly from all buffers
            for (int sender = 0; sender < NUM_NODES; sender++) {
                MessageBuffer* buffer = &message_buffers[sender][node_id];
                
                for (int i = 0; i < buffer->count; i++) {
                    int idx = (buffer->read_index + i) % BUFFER_SIZE;
                    Message* msg = &buffer->messages[idx];
                    
                    if (msg->type == MSG_PROPOSE && msg->round == node->current_round && msg->valid) {
                        propose_count++;
                        if (msg->value == 0 || msg->value == 1) {
                            votes[msg->value]++;
                        }
                    }
                }
            }
            
            // If we have enough propose messages, send prevote
            if (propose_count >= NUM_NODES/2) {
                // Select value with most votes (default to 0 in case of tie)
                int prevote_value = (votes[1] > votes[0]) ? 1 : 0;
                
                sprintf(status_msg, "Received PROPOSE msgs. Prevoting for value: %d (Votes: %d for 0, %d for 1)", 
                        prevote_value, votes[0], votes[1]);
                output_status(node_id, status_msg);
                
                // Send prevote to all nodes
                for (int i = 0; i < NUM_NODES; i++) {
                    // Faulty node might not send prevote or send incorrect value
                    bool be_faulty = node->is_faulty && (get_random_value(100) < FAULTY_PROBABILITY);
                    
                    if (!be_faulty) {
                        send_message(node_id, i, MSG_PREVOTE, node->current_round, prevote_value, true);
                    } else {
                        // Either don't send or send incorrect value
                        if (get_random_value(2) == 0) {
                            sprintf(status_msg, "BYZANTINE BEHAVIOR: Not sending PREVOTE to node %d", i);
                            output_status(node_id, status_msg);
                        } else {
                            int faulty_value = 1 - prevote_value;
                            sprintf(status_msg, "BYZANTINE BEHAVIOR: Sending incorrect PREVOTE %d to node %d", 
                                    faulty_value, i);
                            output_status(node_id, status_msg);
                            send_message(node_id, i, MSG_PREVOTE, node->current_round, faulty_value, true);
                        }
                    }
                }
                
                // Move to precommit state
                node->state = STATE_PRECOMMIT;
                return true;
            }
            
            return false;
        }
            
        case STATE_PRECOMMIT:
            // Wait for enough prevotes (>50%)
            if (node->prevote_count[0] > NUM_NODES/2 || node->prevote_count[1] > NUM_NODES/2) {
                // Determine precommit value
                int precommit_value = -1;
                if (node->prevote_count[0] > NUM_NODES/2) {
                    precommit_value = 0;
                } else if (node->prevote_count[1] > NUM_NODES/2) {
                    precommit_value = 1;
                }
                
                sprintf(status_msg, "Received majority PREVOTES. Precommitting: %d (Prevotes: %d for 0, %d for 1)",
                        precommit_value, node->prevote_count[0], node->prevote_count[1]);
                output_status(node_id, status_msg);
                
                // Send precommit to all nodes
                for (int i = 0; i < NUM_NODES; i++) {
                    // Faulty node behavior
                    bool be_faulty = node->is_faulty && (get_random_value(100) < FAULTY_PROBABILITY);
                    
                    if (!be_faulty || precommit_value == -1) {
                        send_message(node_id, i, MSG_PRECOMMIT, node->current_round, precommit_value, (precommit_value != -1));
                    } else {
                        int faulty_value = 1 - precommit_value;
                        sprintf(status_msg, "BYZANTINE BEHAVIOR: Sending incorrect PRECOMMIT %d to node %d", 
                                faulty_value, i);
                        output_status(node_id, status_msg);
                        send_message(node_id, i, MSG_PRECOMMIT, node->current_round, faulty_value, true);
                    }
                }
                
                // Move to commit state
                node->state = STATE_COMMIT;
                return true;
            }
            return false;
            
        case STATE_COMMIT:
            // Wait for enough precommits (2/3 majority needed)
            if (node->precommit_count[0] > NUM_NODES*2/3 || node->precommit_count[1] > NUM_NODES*2/3) {
                // Determine commit value
                int commit_value = -1;
                if (node->precommit_count[0] > NUM_NODES*2/3) {
                    commit_value = 0;
                } else if (node->precommit_count[1] > NUM_NODES*2/3) {
                    commit_value = 1;
                }
                
                sprintf(status_msg, "Received 2/3+ PRECOMMITS. Committing: %d (Precommits: %d for 0, %d for 1)",
                        commit_value, node->precommit_count[0], node->precommit_count[1]);
                output_status(node_id, status_msg);
                
                // Faulty node might try to commit the wrong value
                if (node->is_faulty && (get_random_value(100) < FAULTY_PROBABILITY) && commit_value != -1) {
                    int faulty_value = 1 - commit_value;
                    sprintf(status_msg, "BYZANTINE BEHAVIOR: Locally committing incorrect value %d", faulty_value);
                    output_status(node_id, status_msg);
                    node->committed_value = faulty_value;
                } else {
                    node->committed_value = commit_value;
                }
                
                // Send commit confirmation to all nodes
                for (int i = 0; i < NUM_NODES; i++) {
                    send_message(node_id, i, MSG_COMMIT, node->current_round, node->committed_value, (node->committed_value != -1));
                }
                
                // Move to decided state
                node->state = STATE_DECIDED;
                return true;
            }
            return false;
            
        case STATE_DECIDED:
            // Check if all nodes have committed
            if (node->commit_count[0] + node->commit_count[1] >= NUM_NODES) {
                sprintf(status_msg, "Round %d complete. Final committed value: %d",
                        node->current_round, node->committed_value);
                output_status(node_id, status_msg);
                
                // Reset for next round
                node->current_round++;
                node->state = STATE_INITIAL;
                
                // Generate new proposed value (could be based on previous round result)
                node->proposed_value = (node->committed_value != -1) ? node->committed_value : (node_id % 2);
                
                // Reset counters
                node->prevote_count[0] = 0;
                node->prevote_count[1] = 0;
                node->precommit_count[0] = 0;
                node->precommit_count[1] = 0;
                node->commit_count[0] = 0;
                node->commit_count[1] = 0;
                
                return true;
            }
            return false;
    }
    
    return false;
}