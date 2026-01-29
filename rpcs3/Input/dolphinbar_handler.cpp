#include "stdafx.h"
#include "dolphinbar_handler.h"
#include "Emu/Io/pad_config.h"
#include <thread>

// Log channel
LOG_CHANNEL(dolphinbar_log, "DolphinBar");

using namespace reports;

dolphinbar_handler::dolphinbar_handler()
	: hid_pad_handler<dolphinbar_device>(pad_handler::dolphinbar, { {0x057e, 0x0306} })
{
	button_list =
	{
		{ dolphinbar_key_codes::none,       "" },
		{ dolphinbar_key_codes::a,          "A (Move)" },
		{ dolphinbar_key_codes::b,          "B (T)" },
		{ dolphinbar_key_codes::one,        "1 (Start)" },
		{ dolphinbar_key_codes::two,        "2 (Select)" },
		{ dolphinbar_key_codes::plus,       "Plus (Triangle)" },
		{ dolphinbar_key_codes::minus,      "Minus (Square)" },
		{ dolphinbar_key_codes::home,       "Home (PS)" },
		{ dolphinbar_key_codes::dpad_left,  "Left" },
		{ dolphinbar_key_codes::dpad_right, "Right" },
		{ dolphinbar_key_codes::dpad_up,    "Up" },
		{ dolphinbar_key_codes::dpad_down,  "Down" },
	};

	init_configs();
	
	m_name_string = "DolphinBar Wiimote #";
	m_max_devices = 4;
	
	// Capabilities
	b_has_config = true;
	b_has_rumble = true;
	b_has_motion = true;
	b_has_deadzones = false; // We process digital buttons mostly
	b_has_led = false;       // Could support player LEDs
	b_has_rgb = false;
	b_has_player_led = true; // Wiimote has 4 LEDs
	b_has_battery = false;   // Could parse battery
	b_has_battery_led = false;
	b_has_pressure_intensity_button = false;
	b_has_orientation = true;
}

dolphinbar_handler::~dolphinbar_handler()
{
}

void dolphinbar_handler::init_config(cfg_pad* cfg)
{
	if (!cfg) return;

	// Set default button mapping
	// Map Wiimote buttons to likely PS Move equivalents
	cfg->cross.def    = ::at32(button_list, dolphinbar_key_codes::a);     // Move button
	cfg->square.def   = ::at32(button_list, dolphinbar_key_codes::minus);
	cfg->circle.def   = ::at32(button_list, dolphinbar_key_codes::plus); 
	cfg->triangle.def = ::at32(button_list, dolphinbar_key_codes::home);  // Or make Home = PS

	// Move controller specific mappings (these are what games see for PS Move)
	cfg->r1.def       = ::at32(button_list, dolphinbar_key_codes::a);
	cfg->r2.def       = ::at32(button_list, dolphinbar_key_codes::b);     // Trigger (T)
	
	cfg->ps.def       = ::at32(button_list, dolphinbar_key_codes::home);
	cfg->start.def    = ::at32(button_list, dolphinbar_key_codes::one);
	cfg->select.def   = ::at32(button_list, dolphinbar_key_codes::two);

	cfg->up.def       = ::at32(button_list, dolphinbar_key_codes::dpad_up);
	cfg->down.def     = ::at32(button_list, dolphinbar_key_codes::dpad_down);
	cfg->left.def     = ::at32(button_list, dolphinbar_key_codes::dpad_left);
	cfg->right.def    = ::at32(button_list, dolphinbar_key_codes::dpad_right);

	// Unmapped by default
	cfg->l1.def       = ::at32(button_list, dolphinbar_key_codes::none);
	cfg->l2.def       = ::at32(button_list, dolphinbar_key_codes::none);
	cfg->r1.def       = ::at32(button_list, dolphinbar_key_codes::none); // Maybe use + or -?

	cfg->orientation_enabled.def = true;
	cfg->from_default();
}

void dolphinbar_handler::check_add_device(hid_device* hidDevice, hid_enumerated_device_view path, std::wstring_view wide_serial)
{
	dolphinbar_log.notice("Initializing DolphinBar Wiimote at %s", std::string(path).c_str());

	// Set non-blocking
	if (hid_set_nonblocking(hidDevice, 1) == -1)
	{
		dolphinbar_log.error("Failed to set non-blocking mode");
		HidDevice::close(hidDevice);
		return;
	}

	// Initialization sequence to enable IR Camera + Accel (Report 0x33)
	// Byte 0 is Report ID for hid_write
	
	const u8 ir_enable[] = {0x13, 0x04};
	const u8 ir_enable2[] = {0x1a, 0x04};
	// Enable IR Camera (reg 0xB00030)
	const u8 enable_cam[] = {0x16, 0x04, 0xb0, 0x00, 0x30, 0x01, 0x08};
	// Sens 1 (Wii Level 5)
	const u8 sens1[] = {0x16, 0x04, 0xb0, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00, 0x41};
	// Sens 2 (Wii Level 5)
	const u8 sens2[] = {0x16, 0x04, 0xb0, 0x00, 0x1a, 0x02, 0x40, 0x00};
	// Mode 3 (Extended) at 0xB00033
	const u8 mode3[] = {0x16, 0x04, 0xb0, 0x00, 0x33, 0x01, 0x03};
	// Report Mode 0x33 (Core + Accel + IR Extended)
	const u8 rpt_mode[] = {0x12, 0x00, 0x33};

	// Helper to send and log errors
	auto send_rpt = [&](const u8* data, size_t len) {
		if (hid_write(hidDevice, data, len) < 0) {
			dolphinbar_log.error("Failed to send init report 0x%02x", data[0]);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20)); // Small delay between commands
	};

	send_rpt(ir_enable, sizeof(ir_enable));
	send_rpt(ir_enable, sizeof(ir_enable)); // Send twice as per protocol
	send_rpt(ir_enable2, sizeof(ir_enable2));
	send_rpt(enable_cam, sizeof(enable_cam));
	send_rpt(sens1, sizeof(sens1));
	send_rpt(sens2, sizeof(sens2));
	send_rpt(mode3, sizeof(mode3));
	send_rpt(rpt_mode, sizeof(rpt_mode));

	dolphinbar_log.success("DolphinBar Wiimote initialized");

	// Add to handler
	// We need to manually add it to m_controllers because hid_pad_handler structure expects us to return or manage it
	// Actually base class calls check_add_device inside enumerate_devices loop.
	// We need to find an empty slot or check if it exists?
	// Base class logic:
	// m_new_enumerated_devices.insert(path);
	// m_devices_to_keep.insert(path);
	// if check_add_device succeeds, it should probably be in m_controllers.
	
	// Create device object
	auto dev = std::make_shared<dolphinbar_device>();
	dev->hidDevice = hidDevice;
	dev->path = path;
	
	std::string path_str(path);
	{
		std::lock_guard lock(m_enumeration_mutex);
		m_controllers[path_str] = dev;
	}
	
	m_new_enumerated_devices.insert(path_str);
	
	// Serial handling
	std::wstring serial_w(wide_serial);
	m_new_enumerated_serials[path_str] = serial_w;
}

int dolphinbar_handler::send_output_report(dolphinbar_device* device)
{
	if (!device || !device->hidDevice) return -1;

	// Handle Rumble and LEDs
	// Report 0x11 is Player LEDs. 0x10 is Rumble?
	// Wiimote Report 0x11: [0x11, rumble_leds]
	// Bits: Rumble(0), LED1(4), LED2(5), LED3(6), LED4(7)
	
	u8 leds = 0;
	if (device->player_id == 0) leds |= 0x10;
	if (device->player_id == 1) leds |= 0x20;
	if (device->player_id == 2) leds |= 0x40;
	if (device->player_id == 3) leds |= 0x80;
	
	bool rumble = (device->large_motor > 0 || device->small_motor > 0);
	if (rumble) leds |= 0x01;
	
	u8 rpt[] = {0x11, leds};
	return hid_write(device->hidDevice, rpt, sizeof(rpt));
}

dolphinbar_handler::DataStatus dolphinbar_handler::get_data(dolphinbar_device* device)
{
	if (!device || !device->hidDevice) return DataStatus::ReadError;

	u8 buf[22]; // Size of Report 0x33 is 18 bytes usually, 22 max
	int res = hid_read(device->hidDevice, buf, sizeof(buf));
	
	if (res < 0) return DataStatus::ReadError;
	if (res == 0) return DataStatus::NoNewData;
	
	// Check Report ID
	if (buf[0] != 0x33) 
	{
		// Maybe receiving status reports or old 0x37? Ignore for now
		return DataStatus::NoNewData;
	}
	
	// Parse 0x33: [ID, Btn1, Btn2, AccX, AccY, AccZ, IR(12)]
	if (res < 18) return DataStatus::NoNewData;
	
	auto& report = device->input_report;
	report.report_id = buf[0];
	report.buttons_1 = buf[1];
	report.buttons_2 = buf[2];
	report.accel_x   = buf[3];
	report.accel_y   = buf[4];
	report.accel_z   = buf[5];
	std::memcpy(report.ir_data, &buf[6], 12);
	
	//dolphinbar_log.trace("Got Report 0x33: Accel(%d %d %d) Btn(%02x %02x)", 
	//	report.accel_x, report.accel_y, report.accel_z, report.buttons_1, report.buttons_2);
		
	return DataStatus::NewData;
}

std::unordered_map<u64, u16> dolphinbar_handler::get_button_values(const std::shared_ptr<PadDevice>& device)
{
	std::unordered_map<u64, u16> key_buf;
	dolphinbar_device* dev = static_cast<dolphinbar_device*>(device.get());
	if (!dev) return key_buf;

	const auto& r = dev->input_report;
	
	// Manual bit parsing to ignore interleaved accelerometer bits
	// Byte 1: Left(0), Right(1), Down(2), Up(3), Plus(4), Accel(5,6), Unknown(7)
	// Byte 2: Two(0), One(1), B(2), A(3), Minus(4), Accel(5,6), Home(7)
	
	u8 b1 = r.buttons_1;
	u8 b2 = r.buttons_2;
	
	key_buf[dolphinbar_key_codes::dpad_left]  = (b1 & 0x01) ? 255 : 0;
	key_buf[dolphinbar_key_codes::dpad_right] = (b1 & 0x02) ? 255 : 0;
	key_buf[dolphinbar_key_codes::dpad_down]  = (b1 & 0x04) ? 255 : 0;
	key_buf[dolphinbar_key_codes::dpad_up]    = (b1 & 0x08) ? 255 : 0;
	key_buf[dolphinbar_key_codes::plus]       = (b1 & 0x10) ? 255 : 0;
	
	key_buf[dolphinbar_key_codes::two]        = (b2 & 0x01) ? 255 : 0;
	key_buf[dolphinbar_key_codes::one]        = (b2 & 0x02) ? 255 : 0;
	key_buf[dolphinbar_key_codes::b]          = (b2 & 0x04) ? 255 : 0;
	key_buf[dolphinbar_key_codes::a]          = (b2 & 0x08) ? 255 : 0;
	key_buf[dolphinbar_key_codes::minus]      = (b2 & 0x10) ? 255 : 0;
	key_buf[dolphinbar_key_codes::home]       = (b2 & 0x80) ? 255 : 0;
	
	return key_buf;
}

pad_preview_values dolphinbar_handler::get_preview_values(const std::unordered_map<u64, u16>& data)
{
	// Simple preview
	return {
		0, 0, 0, 0, 0, 0
	};
}

void dolphinbar_handler::get_extended_info(const pad_ensemble& binding)
{
	const auto& device = binding.device;
	const auto& pad = binding.pad;
	
	dolphinbar_device* dev = static_cast<dolphinbar_device*>(device.get());
	if (!dev || !pad) return;
	
	if (!device->config || !device->config->orientation_enabled)
	{
		pad->move_data.reset_sensors();
		return;
	}
	
	const auto& r = dev->input_report;
	
	// Accelerometer
	// 8-bit values. Center ~128. 1G ~25 units (from our reversing).
	// RPCS3 expects Gs?
	// Using ps_move_handler as ref: values are normalized floats.
	
	// Calibration constants (approximate for now)
	const float center = 128.0f;
	const float scale = 25.0f;
	
	float ax = (static_cast<float>(r.accel_x) - center) / scale;
	float ay = (static_cast<float>(r.accel_y) - center) / scale;
	float az = (static_cast<float>(r.accel_z) - center) / scale;
	
	// Coordinate mapping check:
	// Wiimote held typically:
	// X: Left/Right
	// Y: Forward/Back (Z for GL)
	// Z: Up/Down (Y for GL)
	
	// RPCS3 Move Data:
	pad->move_data.accelerometer.x() = ax;
	pad->move_data.accelerometer.y() = ay;
	pad->move_data.accelerometer.z() = az;
	
	// Gyro: Wiimote has none (unless MotionPlus).
	// For now, zero gyro.
	pad->move_data.gyro.x() = 0.0f;
	pad->move_data.gyro.y() = 0.0f;
	pad->move_data.gyro.z() = 0.0f;
	
	// IR Data
	// Not directly supported in move_data yet? 
	// ps_move_handler calculates "magnetometer" from some fields?
	// RPCS3 likely needs tracking data (position/orientation).
	// Fusion AHRS is used for orientation if we provide sensors.
	
	// If we want to support pointer functionality using IR:
	// We would need to calculate position from IR dots and feed that somewhere.
	// But RPCS3 generally expects raw sensor data for Move.
}
