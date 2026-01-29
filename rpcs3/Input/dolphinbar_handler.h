#pragma once

#include "hid_pad_handler.h"

namespace reports
{
#pragma pack(push, 1)
	struct dolphinbar_input_report
	{
		u8 report_id;   // 0x33
		u8 buttons_1;
		u8 buttons_2;
		u8 accel_x;
		u8 accel_y;
		u8 accel_z;
		u8 ir_data[12]; // 4 objects * 3 bytes
	};
#pragma pack(pop)
}

class dolphinbar_device : public HidDevice
{
public:
	reports::dolphinbar_input_report input_report{};
};

class dolphinbar_handler final : public hid_pad_handler<dolphinbar_device>
{
	enum dolphinbar_key_codes
	{
		none = 0,
		a,
		b,
		one,
		two,
		plus,
		minus,
		home,
		dpad_left,
		dpad_right,
		dpad_up,
		dpad_down,
	};

public:
	dolphinbar_handler();
	~dolphinbar_handler();

	void init_config(cfg_pad* cfg) override;

private:
	DataStatus get_data(dolphinbar_device* device) override;
	void check_add_device(hid_device* hidDevice, hid_enumerated_device_view path, std::wstring_view wide_serial) override;
	int send_output_report(dolphinbar_device* device) override;

	std::unordered_map<u64, u16> get_button_values(const std::shared_ptr<PadDevice>& device) override;
	pad_preview_values get_preview_values(const std::unordered_map<u64, u16>& data) override;
	void get_extended_info(const pad_ensemble& binding) override;
};
