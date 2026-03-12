# USBShield
USBShield protects your Windows system from unauthorized USB devices.

- Automatically **disables unknown USB devices** when inserted  
- Simple and readable **rule** syntax  
- Writes **warning logs** to Event Viewer  
- **Portable** software  


| Comparison        | USBShield | Endpoint Protector | DeviceLock | ManageEngine Device Control | GiliSoft USB Lock |
|------------------------|-----------|--------------------|-----------|----------------------------------|-------------------|
| Paid / Free            | **Free**      | Paid  | Paid | Paid          | Paid |
| Open source            | **Yes**       | No                 | No        | No                               | No                |
| Can block USB devices  | **Yes**       | Yes                | Yes       | Yes                              | Yes               |
| Can log events         | **Yes**       | Yes                | Yes       | Yes                              | Limited    |


----

| - | - |
| -- | -- |
| ![](https://raw.githubusercontent.com/USBShield/USBShield/refs/heads/main/image/1.jpg) | ![](https://raw.githubusercontent.com/USBShield/USBShield/refs/heads/main/image/2.jpg) |
| ![](https://raw.githubusercontent.com/USBShield/USBShield/refs/heads/main/image/3.jpg) | ![](https://raw.githubusercontent.com/USBShield/USBShield/refs/heads/main/image/4.jpg) |


## How to Use
1. Open **Command Prompt** (cmd).  
2. Run `usbshield.exe generate-rules` to create a `rules.conf` file.  
3. Open `rules.conf` and edit it if necessary (see *Rules* below).  
4. Run `usbshield.exe list-devices` to display currently connected devices and see how USBShield will respond.  
5. Run `usbshield.exe watch` in an **elevated** (administrator) Command Prompt to start monitoring. _or register as a Service_


## Rules / Format
- The default policy is: **deny any device not listed in `rules.conf`.** (except HUBs, which are allowed to prevent accidental blocking)
- By default, `allow input` is included - you can remove it for stricter protection.
  - `allow input` allows _input_ devices (such as keyboards or mice) even if they are not listed in `rules.conf`.
- Syntax: `allow {USBID}`
- Use `#` to add comments.
- **It is recommended to review your rules** with `usbshield.exe list-devices` _before_ starting `usbshield.exe watch`.  


## When Mistake Happens
- When USBShield takes an action, it logs the event in Event Viewer.  
- To allow a previously disabled USB device:
  1. Stop USBShield (because it will disable device again)
  2. Open **Device Manager** (`devmgmt.msc`).  
  3. Find the disabled USB device and enable it.  
  4. (Optional) Update your `rules.conf` by running `usbshield.exe generate-rules` again or by editing it manually.  
- Another solution:
  - Stop USBShield temporarily while you're using your computer.
  - Restart USBShield when you are away (e.g., when your computer is locked)


## Optional: Register as a Service
- Use `usbshield.exe service install` to register USBShield as a service, allowing it to monitor your computer in the background.
Other commands:
```
service install     Install the USBShield service
service uninstall   Uninstall the USBShield service
service start       Start the USBShield service
service stop        Stop the USBShield service
```
