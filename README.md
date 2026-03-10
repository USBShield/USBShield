# USBShield
USBShield protects your Windows system from unauthorized USB devices.

- Automatically **disables unknown USB devices** when inserted  
- Simple and readable **rule** syntax  
- Writes **warning logs** to Event Viewer  
- **Portable** software  

| Comparison item        | USBShield | Endpoint Protector | DeviceLock | ManageEngine Device Control Plus | GiliSoft USB Lock |
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
5. Run `usbshield.exe watch` in an **elevated** (administrator) Command Prompt to start monitoring.  

## Rules
- The default policy is: **deny any device not listed in `rules.conf`.**  
- By default, `allow input` is included - you can remove it for stricter protection (recommended).
- `allow input` allows input devices (such as keyboards or mice) even if they are not listed in `rules.conf`.  
- Use `#` to add comments.  
- It is recommended to review your rules with `usbshield.exe list-devices` before starting `usbshield.exe watch`.  

## When Mistake Happens
- When USBShield takes an action, it logs the event in Event Viewer.  
- To allow a previously disabled USB device:  
  1. Open **Device Manager** (`devmgmt.msc`).  
  2. Find the disabled USB device and enable it.  
  3. (Optional) Update your `rules.conf` by running `usbshield.exe generate-rules` again or by editing it manually.  

## Optional: Register as a Service
Use NSSM to register USBShield as a service, allowing it to monitor your computer in the background.
