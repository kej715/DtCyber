#!/bin/bash
echo "Installing NVM"
echo "downloading the installation script and executing through bash"
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.37.2/install.sh | bash
export NVM_DIR="$HOME/.config/nvm"
nvm install node
