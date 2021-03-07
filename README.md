# Arduino for PrograMaker

This implements a bridge that can be run in an Arduino device that supports SSL websocket to connect it to PrograMaker.

## Configuration

To configure the connection copy the `secrets.h.orig` file into a `secrets.h` file. And edit it:

- WiFi network:
    - Set the WIFI network name in `WIFI_SSID`
    - Set the WIFI password in `WIFI_PSK`

- Connection to PrograMaker (to get this information go to PrograMaker.com, open the bridges tab and create a new bridge)
    - Set the bridge URL starting with `/api/v0` on `ENDPOINT_PATH`
    - Set the bridge token in `BRIDGE_TOKEN`

## Adding functionality

The code in this repo only sends a test message with the content "ping" every 0.5 seconds.

Look into `m5stack-example.c` for some examples on how to add more functionalities, specifically how more blocks are configured on `tryConfigure()`. This is a short description of each of the block types:


### Trigger block

Will send a signal to the platform periodically or when a condition happens

```c
    // Define that the output of the signal will be a variable
    signal_argument single_variable_argument = {
        .arg_type=VARIABLE,
        .type=SINGLE,
    };

    // Define the operation
    signal_def sensor_signal = {
        .id="on_sensor_signal",
        .fun_name="on_sensor_signal",
        .key="on_sensor_signal",
        .message="On sensor update. Set %1", // Message in the operation block, %1 will be replaced by the variable dropdown
        .arguments=std::list<signal_argument>({
                single_variable_argument,
            }),
        .save_to={ // Save the result to the first variable
            .index=0
        }
    };
```

This block can be triggered from the Arduino code, with a statement like this:


```c
JSONVar value = JSONVar("just some text"); // Get a value to send
bridge->send_signal("on_sensor_signal", value); // First parameter is the ID of the block defined before
```

### Operation block

Will be used to perform some action on the device

```c
    // Define a string parameter to be used on the blocks
    operation_argument string_argument = {
        .type=STRING,
        .default_value="Hello!",
    };

    // Define the new operation
    operation_def print_line_op = {
        .id="print_line",
        .fun_name="print_line",
        .message="Print line: %1",
        .arguments=std::list<operation_argument>({
                string_argument,
            }),
        .callback=print_line, // Operation to be called when te blocks is run
    };
```

The parameters will be received by the callback as a JSONVar list

```c
// Function to run when "Print line" block is activated
JSONVar print_line(JSONVar arguments) {
    const char* line = (const char*) arguments[0];
    M5.Lcd.println(line);

    return nullptr;
}
```

### Getter block

Will be used to retrieve some value from the device

```c
    getter_def sensor_getter = {
        .id="get_sensors",
        .fun_name="get_sensors",
        .message="Get sensors",
        .arguments=std::list<getter_argument>(), // Functions can have any number of arguments, no arguments is OK too
        .callback=get_sensors, // Function to call to retrieve the values
    };
```


### Configuration

Finally all this blocks are configured into the device with

```c
    bridge = new PrograMaker(webSocket,
                             BRIDGE_TOKEN,
                             "MyDeviceName",
                             std::list<signal_def>({
                                     sensor_signal,
                                     // Other signals
                                 }),
                             std::list<getter_def>({
                                     sensor_getter,
                                     // Other getters
                                 }),
                             std::list<operation_def>({
                                     print_line_op,
                                     // Other operations
                                 }));
```
