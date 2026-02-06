import sdl3
import math
import sys
import ctypes


def main():
    # Initialize SDL
    if not sdl3.SDL_Init(sdl3.SDL_INIT_VIDEO | sdl3.SDL_INIT_GAMEPAD):
        print(f"SDL init failed: {sdl3.SDL_GetError()}")
        sys.exit(1)
    
    # Create window
    window = sdl3.SDL_CreateWindow(
        b"GunCon3 Gyro/Accel Test",
        1280, 720,
        sdl3.SDL_WINDOW_RESIZABLE
    )
    if not window:
        print(f"Window creation failed: {sdl3.SDL_GetError()}")
        sys.exit(1)
    
    # Create renderer
    renderer = sdl3.SDL_CreateRenderer(window, None)
    if not renderer:
        print(f"Renderer creation failed: {sdl3.SDL_GetError()}")
        sys.exit(1)
    
    # Sensor/gamepad state
    gamepad = None
    has_gyro = False
    has_accel = False
    
    # Sensor data (normalized -1.0 to +1.0)
    accel_x = 0.0
    accel_y = 0.0
    accel_z = 0.0
    gyro_x = 0.0
    gyro_y = 0.0
    gyro_z = 0.0
    
    # Gun position state
    gyro_yaw = 0.0
    gyro_pitch = 0.0
    gun_x = 0
    gun_y = 0
    
    # Cursor smoothing
    cursor_history = [(0, 0), (0, 0), (0, 0), (0, 0), (0, 0)]
    
    # Sensitivity settings (matching GunCon3.cpp)
    gyro_sensitivity = 1000.0
    tilt_sensitivity = 65000.0

    # Smoothed sensor data (reverted to raw for calculation)
    # The user wants raw sensor values for calculation but smoothed cursor display
    accel_x = 0.0
    accel_y = 0.0
    accel_z = 0.0
    gyro_x = 0.0
    gyro_y = 0.0
    gyro_z = 0.0
    
    # Try to open gamepad
    count = ctypes.c_int()
    joysticks = sdl3.SDL_GetJoysticks(ctypes.byref(count))
    if joysticks and count.value > 0:
        print(f"Found {count.value} joystick(s)")
        
        for i in range(count.value):
            jid = joysticks[i]
            if sdl3.SDL_IsGamepad(jid):
                gamepad = sdl3.SDL_OpenGamepad(jid)
                if gamepad:
                    name = sdl3.SDL_GetGamepadName(gamepad)
                    if isinstance(name, bytes):
                        name = name.decode('utf-8')
                    print(f"Opened gamepad: {name}")
                    
                    # Check for sensors
                    has_accel = sdl3.SDL_GamepadHasSensor(gamepad, sdl3.SDL_SENSOR_ACCEL)
                    has_gyro = sdl3.SDL_GamepadHasSensor(gamepad, sdl3.SDL_SENSOR_GYRO)
                    
                    print(f"  Accelerometer: {has_accel}")
                    print(f"  Gyroscope: {has_gyro}")
                    
                    # Enable sensors
                    if has_accel:
                        sdl3.SDL_SetGamepadSensorEnabled(gamepad, sdl3.SDL_SENSOR_ACCEL, True)
                    if has_gyro:
                        sdl3.SDL_SetGamepadSensorEnabled(gamepad, sdl3.SDL_SENSOR_GYRO, True)
                    
                    break
    
    if not gamepad:
        print("No gamepad found! Connect a controller with gyro/accel support.")
    
    print("\nControls:")
    print("  SPACE: Reset gyro integration")
    print("  UP/DOWN: Adjust gyro sensitivity")
    print("  LEFT/RIGHT: Adjust tilt sensitivity")
    print("  ESC: Quit")
    print()
    
    running = True
    event = sdl3.SDL_Event()
    
    while running:
        # Handle events
        while sdl3.SDL_PollEvent(event):
            if event.type == sdl3.SDL_EVENT_QUIT:
                running = False
            elif event.type == sdl3.SDL_EVENT_KEY_DOWN:
                if event.key.key == sdl3.SDLK_ESCAPE:
                    running = False
                elif event.key.key == sdl3.SDLK_SPACE:
                    gyro_yaw = 0.0
                    gyro_pitch = 0.0
                    print("Gyro reset!")
                elif event.key.key == sdl3.SDLK_UP:
                    gyro_sensitivity += 1.0
                    print(f"Gyro sensitivity: {gyro_sensitivity}")
                elif event.key.key == sdl3.SDLK_DOWN:
                    gyro_sensitivity = max(1.0, gyro_sensitivity - 1.0)
                    print(f"Gyro sensitivity: {gyro_sensitivity}")
                elif event.key.key == sdl3.SDLK_RIGHT:
                    tilt_sensitivity += 1000.0
                    print(f"Tilt sensitivity: {tilt_sensitivity}")
                elif event.key.key == sdl3.SDLK_LEFT:
                    tilt_sensitivity = max(1000.0, tilt_sensitivity - 1000.0)
                    print(f"Tilt sensitivity: {tilt_sensitivity}")
        
        # Update sensors
        if gamepad:
            # Read accelerometer
            if has_accel:
                data = (ctypes.c_float * 3)()
                if sdl3.SDL_GetGamepadSensorData(gamepad, sdl3.SDL_SENSOR_ACCEL, data, 3):
                    # Normalize by gravity
                    accel_x = data[0] / 9.81
                    accel_y = data[1] / 9.81
                    accel_z = data[2] / 9.81
            
            # Read gyroscope
            if has_gyro:
                data = (ctypes.c_float * 3)()
                if sdl3.SDL_GetGamepadSensorData(gamepad, sdl3.SDL_SENSOR_GYRO, data, 3):
                    gyro_x = data[0]
                    gyro_y = data[1]
                    gyro_z = data[2]
            
            # SIDEWAYS GRIP MAPPING:
            # Use raw gyro_z for yaw when held sideways
            gyro_yaw += gyro_z * gyro_sensitivity
            
            # Use raw accel_x for pitch when held sideways (rotated 90 degrees)
            # Reverse tilt applied
            gyro_pitch = math.atan2(accel_x, math.sqrt(accel_y * accel_y + accel_z * accel_z)) * tilt_sensitivity
            
            # Clamp
            gyro_yaw = max(-32767, min(32767, gyro_yaw))
            gyro_pitch = max(-32767, min(32767, gyro_pitch))
            
            # RAW internal state
            gun_x = int(gyro_yaw)
            gun_y = int(gyro_pitch)
            
            # Cursor smoothing (Moving Average of last 3 points)
            cursor_history.append((gun_x, gun_y))
            cursor_history.pop(0)
            
            # Calculate the "center" (average) of the last 3 points for display
            display_x = sum(p[0] for p in cursor_history) // 3
            display_y = sum(p[1] for p in cursor_history) // 3
            
            # Print values (using display position)
            print(f"Accel: ({accel_x:+.3f}, {accel_y:+.3f}, {accel_z:+.3f})  "
                  f"Gyro Z: {gyro_z:+.3f}  Gun: ({display_x:+6d}, {display_y:+6d})", end='\r')
        
        # Get window size
        w = ctypes.c_int()
        h = ctypes.c_int()
        sdl3.SDL_GetWindowSize(window, ctypes.byref(w), ctypes.byref(h))
        w = w.value
        h = h.value
        
        # Clear screen (dark gray)
        sdl3.SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255)
        sdl3.SDL_RenderClear(renderer)
        
        # Draw grid
        sdl3.SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255)
        for i in range(0, w, 100):
            sdl3.SDL_RenderLine(renderer, i, 0, i, h)
        for i in range(0, h, 100):
            sdl3.SDL_RenderLine(renderer, 0, i, w, i)
        
        # Draw center crosshair
        sdl3.SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255)
        sdl3.SDL_RenderLine(renderer, w//2, 0, w//2, h)
        sdl3.SDL_RenderLine(renderer, 0, h//2, w, h//2)
        
        # Convert gun position to screen coordinates (using smoothed display coordinates)
        cursor_x = int((display_x + 32767) * w / 65535)
        cursor_y = int((display_y + 32767) * h / 65535)
        
        # Draw cursor (red crosshair)
        sdl3.SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255)
        size = 20
        sdl3.SDL_RenderLine(renderer, cursor_x - size, cursor_y, cursor_x + size, cursor_y)
        sdl3.SDL_RenderLine(renderer, cursor_x, cursor_y - size, cursor_x, cursor_y + size)
        
        # Draw circle around cursor
        for angle in range(0, 360, 45):
            rad = math.radians(angle)
            x = cursor_x + int(math.cos(rad) * 10)
            y = cursor_y + int(math.sin(rad) * 10)
            rect = sdl3.SDL_FRect(x - 2, y - 2, 4, 4)
            sdl3.SDL_RenderFillRect(renderer, rect)
        
        # Draw sensor bars
        bar_x = 20
        bar_y = 20
        bar_width = 200
        bar_height = 20
        
        def draw_bar(y_pos, value, r, g, b):
            # Background
            sdl3.SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255)
            rect = sdl3.SDL_FRect(bar_x, y_pos, bar_width, bar_height)
            sdl3.SDL_RenderFillRect(renderer, rect)
            
            # Value bar
            fill_width = value * (bar_width / 2)
            if fill_width > 0:
                sdl3.SDL_SetRenderDrawColor(renderer, r, g, b, 255)
                rect = sdl3.SDL_FRect(bar_x + bar_width/2, y_pos, abs(fill_width), bar_height)
                sdl3.SDL_RenderFillRect(renderer, rect)
            else:
                sdl3.SDL_SetRenderDrawColor(renderer, r, g, b, 255)
                rect = sdl3.SDL_FRect(bar_x + bar_width/2 + fill_width, y_pos, abs(fill_width), bar_height)
                sdl3.SDL_RenderFillRect(renderer, rect)
            
            # Center line
            sdl3.SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255)
            sdl3.SDL_RenderLine(renderer, int(bar_x + bar_width/2), int(y_pos), int(bar_x + bar_width/2), int(y_pos + bar_height))
        
        y = bar_y
        draw_bar(y, accel_x, 255, 100, 100)
        y += bar_height + 25
        draw_bar(y, accel_y, 100, 255, 100)
        y += bar_height + 25
        draw_bar(y, accel_z, 100, 100, 255)
        y += bar_height + 25
        draw_bar(y, gyro_x, 255, 100, 255)
        y += bar_height + 25
        draw_bar(y, gyro_y, 255, 255, 100)
        y += bar_height + 25
        draw_bar(y, gyro_z, 100, 255, 255)
        
        sdl3.SDL_RenderPresent(renderer)
        sdl3.SDL_Delay(16)  # ~60 FPS
    
    print("\nShutting down...")
    
    if gamepad:
        sdl3.SDL_CloseGamepad(gamepad)
    if renderer:
        sdl3.SDL_DestroyRenderer(renderer)
    if window:
        sdl3.SDL_DestroyWindow(window)
    sdl3.SDL_Quit()


if __name__ == "__main__":
    main()
