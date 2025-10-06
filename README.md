# ESP32 Smart WiFi Gateway 🚀

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-v1.0-orange.svg)]()
[![Arduino](https://img.shields.io/badge/Arduino-Compatible-brightgreen.svg)](https://www.arduino.cc/)
[![WiFi](https://img.shields.io/badge/WiFi-Captive%20Portal-yellow.svg)]()
[![Downloads](https://img.shields.io/badge/Downloads-100%2B-brightgreen.svg)]()
[![Last Commit](https://img.shields.io/badge/Last%20Commit-Today-lightgrey.svg)]()
[![Open Source](https://img.shields.io/badge/Open%20Source-Yes!-success.svg)]()

## 📖 Table of Contents
- [Introduction](#-introduction)
- [Features](#-features)
- [Screenshots](#-screenshots)
- [Quick Start](#-quick-start)
- [Installation](#-installation)
- [Configuration](#-configuration)
- [Usage Guide](#-usage-guide)
- [API Reference](#-api-reference)
- [Admin Panel](#-admin-panel)
- [Coming Soon](#-coming-soon)
- [Troubleshooting](#-troubleshooting)
- [Contributing](#-contributing)
- [License](#-license)

## ✨ Introduction

An advanced captive portal system for ESP32 that provides WiFi access through task completion and advertisement viewing. This project transforms your ESP32 into a smart WiFi gateway with comprehensive access control, analytics, and admin capabilities.

> **Note:** 🚧 This project is currently under active development. The core functionality is working, but some features are being enhanced.

## 🎯 Features

### Core Features
- ✅ **Advanced Captive Portal** - Automatic redirection to welcome page
- ✅ **Task-Based Access System** - Complete tasks to earn WiFi time
- ✅ **Multiple Access Packages** - Choose from 5 different task types
- ✅ **Real-time Analytics** - Track usage, devices, and performance
- ✅ **Admin Dashboard** - Comprehensive management interface
- ✅ **Session Management** - Active session monitoring and control
- ✅ **Data Usage Tracking** - Monitor bandwidth consumption
- ✅ **Auto Recovery** - Self-healing system components

### Security & Management
- 🔒 **MAC & IP Whitelisting** - Secure client authentication
- ⏰ **Session Expiry** - Automatic cleanup of expired sessions
- 🔐 **Admin Authentication** - Secure admin panel access
- 🛡️ **Client Isolation** - Per-client access control
- ⚙️ **Remote Configuration** - Web-based settings management

### Analytics & Monitoring
- 📊 **Real-time Statistics** - Live system performance data
- 📱 **Device Detection** - Automatic device type identification
- 💾 **Usage Analytics** - Data consumption per client
- 📈 **Task Completion Tracking** - Performance metrics for each task type
- ❤️ **System Health** - Uptime, memory, and connectivity monitoring

## 🎮 Task Types

| Task | Icon | Duration | Reward | Description |
|------|------|----------|---------|-------------|
| Quick Access | ⚡ | 5 seconds | 15 minutes | Instant access with no requirements |
| Watch Quick Ad | 👀 | 30 seconds | 45 minutes | Short sponsor video |
| YouTube Engagement | 📹 | 30 seconds | 30 minutes | Watch videos & subscribe |
| Instagram Boost | 📸 | 30 seconds | 30 minutes | Like reels & follow channel |
| Premium Ad Watch | 🎬 | 60 seconds | 2 hours | Extended access with premium ad |

## 📸 Screenshots

### Welcome Page
![Welcome Page](https://via.placeholder.com/600x400/667eea/ffffff?text=Welcome+Page+Preview)
*Modern welcome interface with real-time statistics and attractive design*

### Task Selection
![Task Page](https://via.placeholder.com/600x400/ff6b6b/ffffff?text=Task+Selection+Preview)
*Interactive task selection with reward information and progress tracking*

### Admin Dashboard
![Admin Panel](https://via.placeholder.com/600x400/28a745/ffffff?text=Admin+Dashboard+Preview)
*Comprehensive admin interface with system controls and analytics*

### User Status
![User Status](https://via.placeholder.com/600x400/17a2b8/ffffff?text=User+Status+Preview)
*Real-time connection status and usage information for end users*

## 🚀 Quick Start

### Hardware Requirements
- 🖥️ ESP32 Development Board
- 🔌 Micro-USB Cable
- 💻 Computer with Arduino IDE

### Software Requirements
- Arduino IDE 2.0+
- ESP32 Board Support
- Required Libraries (see below)

## ⚡ Installation

### Step 1: Install Arduino IDE
```bash
# Download from https://www.arduino.cc/en/software
# Install according to your operating system
```

### Step 2: Install ESP32 Board Support
1. Open Arduino IDE
2. Go to `File > Preferences`
3. Add to Additional Board Manager URLs:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to `Tools > Board > Board Manager`
5. Search for "esp32" and install

### Step 3: Install Required Libraries
```cpp
// Install these libraries via Library Manager (Tools > Manage Libraries)
- WiFi
- WebServer
- DNSServer
- SPIFFS
- ArduinoJson
- Update
```

### Step 4: Upload Code
```cpp
// 1. Clone this repository
git clone https://github.com/jvdhussain026/esp32-smart-wifi-gateway.git

// 2. Open v0.3.ino in Arduino IDE
// 3. Select your ESP32 board (Tools > Board)
// 4. Choose correct COM port (Tools > Port)
// 5. Click Upload button
```

### Step 5: First Time Setup
1. 📶 Connect to "Free WiFi Gateway" access point
2. 🌐 Open browser to `http://192.168.4.1`
3. 🎯 Complete the setup wizard
4. 🔧 Configure admin settings

## ⚙️ Configuration

### Basic Settings
Edit these constants in the code:

```cpp
// WiFi Configuration
const char* AP_SSID = "Free WiFi Gateway";
const char* AP_PASSWORD = ""; // Leave empty for open network

// Admin Configuration
const char* ADMIN_USER = "admin";
const char* ADMIN_PASS = "admin123";

// Uplink Configuration
const char* UPLINK_SSID_DEFAULT = "your_home_wifi";
const char* UPLINK_PASS_DEFAULT = "your_wifi_password";
```

### Task Configuration
Modify task durations and rewards:

```cpp
// Task durations in milliseconds
const unsigned long TASK_SIMPLE_DURATION = 15 * 60 * 1000UL; // 15 minutes
const unsigned long TASK_AD_30_SEC_DURATION = 45 * 60 * 1000UL; // 45 minutes
const unsigned long TASK_YOUTUBE_DURATION = 30 * 60 * 1000UL; // 30 minutes
const unsigned long TASK_INSTAGRAM_DURATION = 30 * 60 * 1000UL; // 30 minutes
const unsigned long TASK_AD_60_SEC_DURATION = 2 * 60 * 60 * 1000UL; // 2 hours
```

## 🎯 Usage Guide

### For End Users 👥

#### Step 1: Connect to WiFi
```bash
# Scan for available WiFi networks
# Connect to "Free WiFi Gateway"
# Password: (leave empty for open network)
```

#### Step 2: Open Browser
```bash
# Browser automatically redirects to welcome page
# If not, manually visit: http://192.168.4.1
```

#### Step 3: Choose Task
```bash
# Click "Get Free Access" button
# Select from available tasks based on desired access time
# Popular choice: 🎬 Premium Ad Watch (2 hours access)
```

#### Step 4: Complete Task
```bash
# Follow on-screen instructions
# Watch ad, engage with content, or complete simple task
# Wait for timer to complete
```

#### Step 5: Get Access
```bash
# Automatic WiFi access upon task completion
# Check status at: http://192.168.4.1/userstatus
# Monitor remaining time and data usage
```

### For Administrators 🛠️

#### Step 1: Access Admin Panel
```bash
# Navigate to: http://192.168.4.1/admin
# Login with credentials (default: admin/admin123)
```

#### Step 2: Monitor System
```bash
# View real-time statistics
# Monitor active sessions
# Check system health
```

#### Step 3: Manage Settings
```bash
# Configure WiFi settings
# Adjust task parameters
# Update system configurations
```

#### Step 4: Control Access
```bash
# Kick unwanted users
# Restart system if needed
# View detailed analytics
```

## 🔌 API Reference

### Core Endpoints

| Endpoint | Method | Description | Parameters |
|----------|--------|-------------|------------|
| `/` | GET | Welcome page & captive portal | None |
| `/tasks` | GET | Task selection interface | None |
| `/dotask` | GET | Individual task page | `task` (task ID) |
| `/granttask` | POST | Grant access after task completion | `task` (task ID) |
| `/userstatus` | GET | User connection status | None |
| `/status` | GET | Network status page | None |
| `/admin` | GET/POST | Admin dashboard | User/Pass (for POST) |
| `/api/stats` | GET | JSON statistics API | None |

### Admin Endpoints

| Endpoint | Method | Description | Parameters |
|----------|--------|-------------|------------|
| `/admin/update` | POST | Update system settings | Various settings |
| `/admin/kick` | GET | Kick specific user | `ip` (IP address) |
| `/admin/kickall` | GET | Kick all users | None |
| `/admin/restart` | GET | Restart system | None |

### Example API Usage
```javascript
// Get system statistics
fetch('/api/stats')
  .then(response => response.json())
  .then(data => {
    console.log('Active sessions:', data.activeSessions);
    console.log('System uptime:', data.uptime);
  });

// Kick specific user
fetch('/admin/kick?ip=192.168.4.2')
  .then(response => response.text())
  .then(result => {
    console.log('Kick result:', result);
  });
```

## 🛠️ Admin Panel

### Dashboard Overview
The admin panel provides comprehensive system management through five main tabs:

#### 📊 Overview Tab
- System statistics and metrics
- Quick action buttons
- Real-time performance data
- System health monitoring

#### 👥 Sessions Tab
- Active session management
- Client information (IP, device, package)
- Session duration tracking
- Manual session termination

#### ⚙️ Settings Tab
- WiFi configuration (AP & STA)
- System parameter adjustments
- Task configuration
- Security settings

#### 💻 System Tab
- Hardware information
- Network status
- Memory usage
- Connectivity details

#### 📈 Analytics Tab
- Task completion statistics
- Device type distribution
- Usage patterns
- Performance metrics

### Admin Features
- **Real-time Monitoring**: Live updates of all system activities
- **Session Control**: Individual or bulk session management
- **Configuration Management**: Web-based system configuration
- **Performance Analytics**: Detailed usage and performance data
- **System Maintenance**: Restart and maintenance operations

## 🔮 Coming Soon

### 🤖 Telegram Integration
- **Bot Notifications**: Real-time alerts for new connections and system events
- **Remote Administration**: Manage system via Telegram commands
- **Automated Reports**: Daily/weekly usage statistics sent via Telegram
- **Quick Controls**: Kick users, restart system, view status via bot

### 🌐 Enhanced Web Features
- **Multi-language Support**: Internationalization for global use
- **Custom Themes**: User-selectable UI themes and colors
- **Progressive Web App**: Mobile-optimized PWA experience
- **Offline Functionality**: Basic functionality without internet

### 🔒 Advanced Security
- **OAuth Integration**: Social media authentication options
- **Rate Limiting**: Prevent system abuse and spam
- **Advanced Firewall**: IP/MAC filtering with custom rules
- **SSL Support**: HTTPS encryption for secure communication

### 📊 Business Features
- **Dynamic Ad Rotation**: Automated advertisement management
- **Payment Integration**: Premium access and payment options
- **Sponsor Portal**: Dedicated dashboard for advertisers
- **Advanced Analytics**: Business intelligence and reporting

### 🔧 Technical Enhancements
- **Database Integration**: External database support for large deployments
- **API Extensions**: RESTful API for third-party integrations
- **Cluster Support**: Multiple ESP32 units working together
- **Mobile App**: Dedicated mobile application for management

## 🐛 Troubleshooting

### Common Issues & Solutions

#### ❌ AP Not Starting
**Problem**: ESP32 access point not visible
```cpp
// Solution: Check WiFi mode and credentials
WiFi.mode(WIFI_AP_STA);
WiFi.softAP(AP_SSID, AP_PASSWORD);
```

#### ❌ Portal Not Redirecting
**Problem**: Browser doesn't redirect to captive portal
```bash
# Solution: Clear browser cache or try manual URL
http://192.168.4.1
```

#### ❌ Admin Login Fails
**Problem**: Cannot access admin panel
```cpp
// Solution: Verify credentials in code
const char* ADMIN_USER = "admin";
const char* ADMIN_PASS = "admin123";
```

#### ❌ Memory Issues
**Problem**: System crashes or behaves unpredictably
```cpp
// Solution: Reduce session limits or optimize memory
#define MAX_SESSIONS 20  // Reduce from 50 if needed
```

### Debug Mode
Enable detailed logging for troubleshooting:

```cpp
#define SERIAL_BAUD_RATE 115200

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  // Debug information will appear in Serial Monitor
}

// Open Serial Monitor at 115200 baud to see logs
```

### Performance Optimization Tips
- 💡 Use stable power supply (2A recommended)
- 💡 Ensure good WiFi signal strength
- 💡 Monitor memory usage in admin panel
- 💡 Regular system restarts for maintenance
- 💡 Limit concurrent users based on ESP32 model

## 📈 Performance Specifications

### System Requirements
| Component | Specification | Notes |
|-----------|---------------|-------|
| ESP32 Module | Any ESP32 with WiFi | ESP32-WROOM recommended |
| Flash Size | 4MB+ | 16MB for advanced features |
| RAM Usage | Optimized | Monitor via admin panel |
| Concurrent Users | 5-10 recommended | Maximum 50 sessions |

### Resource Usage
- **Memory**: Optimized for ESP32 constraints
- **Storage**: Efficient SPIFFS usage for data
- **Network**: Handles multiple concurrent connections
- **Power**: Efficient operation for continuous use

## 🤝 Contributing

We love your input! We want to make contributing to this project as easy and transparent as possible.

### How to Contribute

1. **Fork the Repository**
   ```bash
   # Click 'Fork' button on GitHub
   git clone https://github.com/YOUR_USERNAME/esp32-smart-wifi-gateway.git
   ```

2. **Create a Feature Branch**
   ```bash
   git checkout -b feature/AmazingFeature
   ```

3. **Make Your Changes**
   ```bash
   # Add your amazing features
   # Test thoroughly
   git add .
   git commit -m "Add some AmazingFeature"
   ```

4. **Push and Create Pull Request**
   ```bash
   git push origin feature/AmazingFeature
   # Create PR on GitHub
   ```

### Development Guidelines
- 📝 Follow existing code style and formatting
- 💬 Add comments for new features and complex logic
- 🧪 Test thoroughly on actual ESP32 hardware
- 📚 Update documentation for new features
- 🐛 Create issues for bugs before fixing

### Project Structure
```
esp32-smart-wifi-gateway/
├── 📁 src/
│   └── v0.3.ino              # Main application code
├── 📁 docs/                  # Documentation
├── 📁 screenshots/           # Project screenshots
├── 📁 examples/              # Usage examples
├── 📁 hardware/              # Hardware designs
├── 📄 LICENSE               # MIT License
├── 📄 README.md             # This file
└── 📄 CONTRIBUTING.md       # Contribution guidelines
```

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

```text
MIT License

Copyright (c) 2024 Javed Hussain

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 🙏 Acknowledgments

- **ESP32 Community** - For excellent documentation and support
- **Arduino Ecosystem** - Robust libraries and development environment
- **Open Source Contributors** - Everyone who helped improve this project
- **Testers and Users** - Valuable feedback and bug reports

### Special Thanks
- ESP32 core development team
- ArduinoJson library maintainers
- WiFi and WebServer library contributors
- All our GitHub stars and contributors!

## 📞 Support

### Getting Help
- **GitHub Issues**: [Create an issue](https://github.com/jvdhussain026/esp32-smart-wifi-gateway/issues)
- **Email**: jvdhussain026@gmail.com
- **Documentation**: [Project Wiki](https://github.com/jvdhussain026/esp32-smart-wifi-gateway/wiki)

### Resources
- 📚 [ESP32 Official Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- 💬 [Arduino Forum](https://forum.arduino.cc/)
- 🐛 [Issue Tracker](https://github.com/jvdhussain026/esp32-smart-wifi-gateway/issues)
- 💡 [Examples & Tutorials](https://github.com/jvdhussain026/esp32-smart-wifi-gateway/examples)

### Community
- ⭐ Star the repository if you find it helpful!
- 🔄 Share with others who might benefit
- 💭 Suggest new features and improvements
- 🐛 Report bugs and issues you encounter

---

<div align="center">

## 🚀 Ready to Get Started?

[![Download Now](https://img.shields.io/badge/Download-Latest-blue.svg)](https://github.com/jvdhussain026/esp32-smart-wifi-gateway/archive/main.zip)
[![View Code](https://img.shields.io/badge/View-Code-success.svg)](v0.3.ino)
[![Report Bug](https://img.shields.io/badge/Report-Bug-red.svg)](https://github.com/jvdhussain026/esp32-smart-wifi-gateway/issues)
[![Request Feature](https://img.shields.io/badge/Request-Feature-green.svg)](https://github.com/jvdhussain026/esp32-smart-wifi-gateway/issues)

### ⭐ Don't forget to star this repository if you found it useful!

**Made with ❤️ by [Javed Hussain](https://github.com/jvdhussain026)**

[![Follow on GitHub](https://img.shields.io/badge/Follow-GitHub-black.svg)](https://github.com/jvdhussain026)
[![Visit Repository](https://img.shields.io/badge/Visit-Repository-important.svg)](https://github.com/jvdhussain026/esp32-smart-wifi-gateway)

</div>

---

*Last updated: ${new Date().toLocaleDateString()}*
