#!/bin/bash
# install.sh - Installation script for Snapcast
# Automatically installs or replaces binaries built with build.sh

# Define colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to print colored messages
print_status() {
  local color=$1
  local message=$2
  echo -e "${color}${message}${NC}"
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then
  print_status "$RED" "Please run as root (sudo ./install.sh)"
  exit 1
fi

# Check if binaries exist
if [ ! -f "bin/snapclient" ] || [ ! -f "bin/snapserver" ]; then
  print_status "$RED" "Binaries not found in bin/ directory!"
  print_status "$YELLOW" "Please run ./build.sh first to build the binaries."
  exit 1
fi

# Create directories if they don't exist
print_status "$GREEN" "Creating installation directories..."
mkdir -p /usr/bin
mkdir -p /etc/snapserver
mkdir -p /usr/share/snapserver

# Check for existing binaries and back them up if found
if [ -f "/usr/bin/snapclient" ]; then
  print_status "$YELLOW" "Backing up existing snapclient..."
  mv /usr/bin/snapclient /usr/bin/snapclient.bak
fi

if [ -f "/usr/bin/snapserver" ]; then
  print_status "$YELLOW" "Backing up existing snapserver..."
  mv /usr/bin/snapserver /usr/bin/snapserver.bak
fi

# Copy binaries
print_status "$GREEN" "Installing binaries..."
cp bin/snapclient /usr/bin/
cp bin/snapserver /usr/bin/

# Set permissions
chmod 755 /usr/bin/snapclient
chmod 755 /usr/bin/snapserver

# Copy configuration files if they exist and destination doesn't
if [ -f "server/etc/snapserver.conf" ] && [ ! -f "/etc/snapserver/snapserver.conf" ]; then
  print_status "$GREEN" "Installing snapserver.conf..."
  cp server/etc/snapserver.conf /etc/snapserver/
elif [ -f "server/etc/snapserver.conf" ]; then
  print_status "$YELLOW" "Config file already exists, not overwriting."
  print_status "$YELLOW" "If you want to update the config, use:"
  print_status "$YELLOW" "  sudo cp server/etc/snapserver.conf /etc/snapserver/"
fi

# Check if systemd service files exist and offer to install them
if [ -f "server/etc/snapserver.service" ]; then
  if [ ! -f "/etc/systemd/system/snapserver.service" ]; then
    print_status "$GREEN" "Installing snapserver systemd service..."
    cp server/etc/snapserver.service /etc/systemd/system/
    systemctl daemon-reload
  else
    print_status "$YELLOW" "Snapserver service already exists, not overwriting."
  fi
fi

if [ -f "client/etc/snapclient.service" ]; then
  if [ ! -f "/etc/systemd/system/snapclient.service" ]; then
    print_status "$GREEN" "Installing snapclient systemd service..."
    cp client/etc/snapclient.service /etc/systemd/system/
    systemctl daemon-reload
  else
    print_status "$YELLOW" "Snapclient service already exists, not overwriting."
  fi
fi

systemctl daemon-reload

print_status "$GREEN" "Installation complete!"
print_status "$GREEN" "You can now run snapserver and snapclient from anywhere."
print_status "$GREEN" "To start the services, run:"
print_status "$GREEN" "  sudo systemctl start snapserver"
print_status "$GREEN" "  sudo systemctl start snapclient"
print_status "$GREEN" "To enable them at boot:"
print_status "$GREEN" "  sudo systemctl enable snapserver"
print_status "$GREEN" "  sudo systemctl enable snapclient"
