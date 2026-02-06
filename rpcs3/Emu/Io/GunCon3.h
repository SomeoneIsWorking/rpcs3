#pragma once

#include "Emu/Io/usb_device.h"

class usb_device_guncon3 : public usb_device_emulated
{
public:
	usb_device_guncon3(u32 controller_index, const std::array<u8, 7>& location);
	~usb_device_guncon3();

	void control_transfer(u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength, u32 buf_size, u8* buf, UsbTransfer* transfer) override;
	void interrupt_transfer(u32 buf_size, u8* buf, u32 endpoint, UsbTransfer* transfer) override;

private:
	u32 m_controller_index;
	std::array<u8, 8> m_key{};

	// Gyro/Accel tracking for aiming
	f32 m_gyro_pitch = 0.0f;  // X-axis rotation
	f32 m_gyro_yaw = 0.0f;    // Y-axis rotation
	f32 m_accel_x = 0.0f;
	f32 m_accel_y = 0.0f;
	f32 m_accel_z = 0.0f;
	s16 m_gun_x = 0;
	s16 m_gun_y = 0;
	bool m_use_gyro_aiming = true;  // Toggle for gyro-based aiming
};
