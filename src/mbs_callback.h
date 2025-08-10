#ifndef MBS_CALLBACK_H
#define MBS_CALLBACK_H

#include <Arduino.h>
#include "modbus/nanomodbus/nanomodbus.h"


//Callback functions called by server
nmbs_error handle_read_device_identification(uint8_t object_id, char buffer[NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH]);
nmbs_error handle_read_device_identification_map(nmbs_bitfield_256 map);
nmbs_error handle_write_file_record(uint16_t file_number, uint16_t record_number, const uint16_t *registers, uint16_t count, uint8_t unit_id);
nmbs_error handle_read_file_record(uint16_t file_number, uint16_t record_number, uint16_t *registers, uint16_t count, uint8_t unit_id);
nmbs_error handle_write_multiple_registers(uint16_t address, uint16_t quantity, const uint16_t *registers, uint8_t unit_id);
nmbs_error handle_write_multiple_coils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, uint8_t unit_id);
nmbs_error handle_write_single_register(uint16_t address, uint16_t value, uint8_t unit_id);
nmbs_error handle_write_single_coil(uint16_t address, bool value, uint8_t unit_id);
nmbs_error handle_read_input_registers(uint16_t address, uint16_t quantity, uint16_t *registers_out, uint8_t unit_id);
nmbs_error handle_read_holding_registers(uint16_t address, uint16_t quantity, uint16_t *registers_out, uint8_t unit_id);
nmbs_error handle_read_discrete_inputs(uint16_t address, uint16_t quantity, nmbs_bitfield inputs_out, uint8_t unit_id);
nmbs_error handle_read_coils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id);

bool modbus_set_digital_outputs_on_server(const uint8_t outputs[], const uint16_t address, const uint16_t quantity);
bool modbus_set_digital_inputs_on_server(const uint8_t inputs[], const uint16_t address, const uint16_t quantity);
bool modbus_set_analog_inputs_on_server(const uint16_t inputs[], const uint16_t address, const uint16_t quantity);
uint16_t *modbus_get_analog_inputs_on_server();
bool modbus_get_parameters_at_server(uint16_t parameters[], const uint16_t address, const uint16_t quantity);
bool modbus_set_parameters_on_server(const uint16_t parameters[], const uint16_t address, const uint16_t quantity);
uint16_t *modbus_get_parameters_on_server();




//Callback functions called by client
// No required for client





// Serial read/write callback functions shared by both client and server
int32_t read_serial(const char port[], uint8_t *buf, uint16_t count, int32_t byte_timeout_ms);
int32_t write_serial(const char port[], const uint8_t *buf, uint16_t count, int32_t byte_timeout_ms);

#endif // MBS_CALLBACK_H