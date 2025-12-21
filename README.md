## Shellpower ++
Hello!

WELCOME to shellpower plus plus
This project is a crossplatform port of the orginal sscp [shellpower](https://github.com/sscp/shellpower) with additional features such as auto array layout
<img width="1469" height="887" alt="Screenshot 2025-12-21 at 11 59 57 AM" src="https://github.com/user-attachments/assets/6af733d7-d40d-4f50-817b-1594add23813" />

## Installation
Downad the release for your computer over in [releases](https://github.com/Jerasaurus/shellpowerplusplus/releases/latest)

## Usage

1) OBJ or STL works, not STEP, but if you can export from cad as OBJ it would be ideal


### 2) Linux

```bash
cd Downloads # or wherever you downloaded the executable
chmod +x shellpower-linux-x64
./shellpower-linux-x64
```
### 2) Mac
Open Terminal Application
```bash
cd Downloads # or wherever you downloaded the executable
chmod +x shellpower-macos-arm64
xattr -d com.apple.quarantine shellpower-macos-arm64
./shellpower-macos-arm64
```
## 2) Windows
```bash
#double click!
.\shellpower-windows-x64.exe
```
3) click "Load mesh file" and import your body! This needs to be an OBJ OR STL
4) transform so the array surface is facing up ( y axis or the green arrow), MAKE SURE SCALE IS SET SO THE SIZE DISPLAYED IS IN METERS
5) go over to cells, (your view will change to vertical, set `Target` to how many sqm of array you want to try and fit on the car.
6) Dont change any of the checkboxes from defaults and hit `Run Auto-Layout`
7) Go over to simulate, and you can drag around the `hour` slider to see the array preformance at instantious points during the day.
8) To get a time averaged sim, hit `Run Daily Simulation`
## Credit
Credit to DC for the orginal C# project! [https://github.com/sscp/shellpower](https://github.com/sscp/shellpower)
