## Shellpower ++
Hello!

WELCOME to shellpower plus plus

## Installation
Downad the release for your computer over in [releases](https://github.com/Jerasaurus/shellpowerplusplus/releases/latest)

## Usage

1) you need to convert your model to OBJ ( you can use any cloud converter )


### 2) Linux

```bash
chmod +x shellpower-linux-x64
./shellpower-linux-x64
```
### 2) Mac

```bash
chmod +x shellpower-macos-arm64
xattr -d com.apple.quarantine shellpower-macos-arm64
./shellpower-macos-arm64
```
## 2) Windows
```bash
#double click!
.\shellpower-windows-x64.exe
```
3) click "Load mesh file" and import your body! This needs to be an OBJ.
4) transform so the array surface is facing up ( y axis or the green arrow)
5) go over to cells, (your view will change to vertical, set `Target` to how many sqm of array you want to try and fit on the car.
6) Dont change any of the checkboxes from defaults and hit `Run Auto-Layout`
7) Go over to simulate, and you can drag around the `hour` slider to see the array preformance at instantious points during the day.
8) To get a time averaged sim, hit `Run Daily Simulation`
