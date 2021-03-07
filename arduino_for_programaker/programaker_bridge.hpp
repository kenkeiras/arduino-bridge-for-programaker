#include <Arduino_JSON.h>
#include <list>

enum ARGUMENT_TYPE {
    VARIABLE,
};

enum VALUE_ARGUMENT_TYPE {
    STRING,
    INTEGER,
    FLOAT,
    BOOLEAN,
};

enum VARIABLE_TYPE {
    SINGLE,
    LIST,
};

typedef struct {
    enum ARGUMENT_TYPE arg_type;
    enum VARIABLE_TYPE type;
} signal_argument;

typedef struct {
    enum VALUE_ARGUMENT_TYPE type;
    char* default_value;
} getter_argument;

typedef struct {
    enum VALUE_ARGUMENT_TYPE type;
    char* default_value;
} operation_argument;

typedef struct {
    // Type not included
    int index;
} argument_reference;

typedef struct {
    char* id;
    char* fun_name;
    char* key;
    String message;
    std::list<signal_argument> arguments;
    argument_reference save_to;
} signal_def;

typedef struct {
    char* id;
    char* fun_name;
    String message;
    std::list<getter_argument> arguments;

    // enum BLOCK_RESULT_TYPE result_type;
    JSONVar (*callback) (JSONVar); // Pointer to the callback function
} getter_def;

typedef struct {
    char* id;
    char* fun_name;
    String message;
    std::list<operation_argument> arguments;

    // enum BLOCK_RESULT_TYPE result_type;
    JSONVar (*callback) (JSONVar); // Pointer to the callback function
} operation_def;

typedef struct {
    char* function_name;
    JSONVar (*callback) (JSONVar);
} callback_register;

class ProgramakerBridge {
    WebSocketsClient *ws;
    std::list<callback_register> callbacks;

public:
    ProgramakerBridge(WebSocketsClient *ws,
                      String auth_token,
                      String name,
                      std::list<signal_def> signals,
                      std::list<getter_def> getters,
                      std::list<operation_def> operations) {
        this->ws = ws;
        this->auth(auth_token);
        this->configure(name, signals, getters, operations);
    }

    void loop() {
        do {
            this->responses_in_loop = false;
            this->ws->loop();
        } while (this->responses_in_loop);
    }

    void send_signal(String key, JSONVar value){
        JSONVar doc;
        JSONVar to_user; // Null
        doc["type"] = "NOTIFICATION";
        doc["key"] = key;
        doc["to_user"] = to_user;

        // @TODO Separate content and value
        doc["content"] = value;
        doc["value"] = value;

        Serial.println(doc);

        String jsonString = JSON.stringify(doc);
        this->ws->sendTXT(jsonString);
    }

    void on_received_text(char* text, size_t length) {
        this->responses_in_loop = false;

        text[length] = '\0';
        Serial.printf("Received: %s\n", text);
        JSONVar var = JSON.parse((char*)text);
        const char* type = (const char*) var["type"];
        const char* message_id = (const char*) var["message_id"];
        if (strcmp(type, "FUNCTION_CALL") == 0){
            JSONVar value = var["value"];
            const char* function_name = (const char*) value["function_name"];
            for(const auto& callback : callbacks) {
                if (strcmp(function_name, callback.function_name) == 0) {
                    JSONVar result = callback.callback(value["arguments"]);

                    JSONVar response;
                    response["message_id"] = message_id;
                    response["success"] = true;
                    response["result"] = result;

                    String jsonString = JSON.stringify(response);
                    this->ws->sendTXT(jsonString);

                    break;
                }
            }
        }
        else if (strcmp(type, "GET_HOW_TO_SERVICE_REGISTRATION") == 0){
            JSONVar response;
            response["message_id"] = message_id;
            response["success"] = true;
            response["result"] = nullptr;

            String jsonString = JSON.stringify(response);
            this->ws->sendTXT(jsonString);
        }
        else if (strcmp(type, "REGISTRATION") == 0){
            JSONVar response;
            response["message_id"] = message_id;
            response["success"] = true;
            response["result"] = nullptr;

            String jsonString = JSON.stringify(response);
            this->ws->sendTXT(jsonString);
        }
    }

private:
    bool responses_in_loop = false;

    void auth(String auth_token) {
        // Build auth message
        JSONVar doc;
        doc["type"] = "AUTHENTICATION";

        JSONVar value;
        value["token"] = auth_token;

        doc["value"] = value;

        // Send message
        String jsonString = JSON.stringify(doc);
        this->ws->sendTXT(jsonString);
        Serial.println("SENT AUTHENTICATION");
    }

    void configure(String name,
                   std::list<signal_def> signals,
                   std::list<getter_def> getters,
                   std::list<operation_def> operations){
        JSONVar doc;
        doc["type"] = "CONFIGURATION";

        JSONVar value;
        value["is_public"] = false;
        value["service_name"]  = name;

        JSONVar icon;
        icon["url"] = "https://avatars.githubusercontent.com/u/9460735";

        value["icon"] = icon;
        JSONVar blocks = JSON.parse("[]");
        int block_count = 0;
        for (const auto& signal : signals) {
            JSONVar block;
            block["id"] = signal.id;
            block["function_name"] = signal.fun_name;
            block["key"] = signal.key;
            block["block_type"] = "trigger";
            block["message"] = signal.message;
            block["expected_value"] = nullptr;

            JSONVar arguments = JSON.parse("[]");

            int arg_count = 0;
            for(const auto& arg : signal.arguments) {
                JSONVar argument;

                switch(arg.arg_type) {
                case VARIABLE:
                    argument["type"] = "variable";
                    break;
                }

                switch (arg.type) {
                case SINGLE:
                    argument["class"] = "single";
                    break;
                case LIST:
                    argument["class"] = "list";
                    break;
                }

                arguments[arg_count] = argument;
                arg_count++;
            }

            int save_to_index = signal.save_to.index;
            if ((save_to_index < 0) ||
                (save_to_index > arg_count)) {
                block["save_to"] = nullptr;
            }
            else {
                JSONVar save_to;
                save_to["type"] = "argument";
                save_to["index"] = save_to_index;
                block["save_to"] = save_to;
            }

            block["arguments"] = arguments;
            blocks[block_count] = block;
            block_count++;
        }

        for (const auto& getter : getters) {
            JSONVar block;

            block["id"] = getter.id;
            block["function_name"] = getter.fun_name;
            block["block_type"] = "getter";
            block["block_result_type"] = nullptr;
            block["message"] = getter.message;

            JSONVar arguments = JSON.parse("[]");
            int arg_count = 0;
            for(const auto& arg : getter.arguments) {
                JSONVar argument;

                switch(arg.type) {
                case STRING:
                    argument["type"] = "string";
                    break;
                case INTEGER:
                    argument["type"] = "integer";
                    break;
                case FLOAT:
                    argument["type"] = "float";
                    break;
                case BOOLEAN:
                    argument["type"] = "boolean";
                    break;
                }
                argument["default"] = arg.default_value;

                arguments[arg_count] = argument;
                arg_count++;
            }

            block["arguments"] = arguments;
            blocks[block_count] = block;
            block_count++;

            callbacks.push_back({
                    .function_name=getter.fun_name,
                    .callback=getter.callback,
                });
        }

        for (const auto& operation : operations) {
            JSONVar block;

            block["id"] = operation.id;
            block["function_name"] = operation.fun_name;
            block["block_type"] = "operation";
            block["block_result_type"] = nullptr;
            block["message"] = operation.message;

            JSONVar arguments = JSON.parse("[]");

            int arg_count = 0;
            for(const auto& arg : operation.arguments) {
                JSONVar argument;

                switch(arg.type) {
                case STRING:
                    argument["type"] = "string";
                    break;
                case INTEGER:
                    argument["type"] = "integer";
                    break;
                case FLOAT:
                    argument["type"] = "float";
                    break;
                case BOOLEAN:
                    argument["type"] = "boolean";
                    break;
                }
                argument["default"] = arg.default_value;

                arguments[arg_count] = argument;
                arg_count++;
            }

            block["arguments"] = arguments;
            blocks[block_count] = block;
            block_count++;

            callbacks.push_back({
                    .function_name=operation.fun_name,
                    .callback=operation.callback,
                });
        }

        value["blocks"] = blocks;
        doc["value"] = value;

        Serial.println(doc);

        String jsonString = JSON.stringify(doc);
        Serial.println(jsonString);
        this->ws->sendTXT(jsonString);
        Serial.println("OK");
    }
};
