const net = require('net');

class Machine {
    constructor(id, url, port) {
        this.id = id;
        this.debug = false;
        this.url = url;
        this.port = port;
        this.END_CHARACTER = 0xFF; // End character
        this.buffer = Buffer.alloc(0);
        this.client = null;
        this.isConnect = false;
        this.allowReconnect = true;
        this.reconnectInterval = 1000; // Reconnect interval in milliseconds
        this.reconnecting = false; // To track if reconnection is in progress

        this.connectListener = null;
        this.disconnectListener = null;
        this.reconnectStartListener = null;
        this.receivedDataHandler = null;
    }

    setConnectListener(callback) {
        this.connectListener = callback;
    }

    setDisconnectListener(callback) {
        this.disconnectListener = callback;
    }

    setReconnectStartListener(callback) {
        this.reconnectStartListener = callback;
    }

    setReceivedDataHandler(receivedDataHandler) {
        this.receivedDataHandler = receivedDataHandler;
    }

    setTerminal(dummy) {
    }

    createConnection() {
        if (this.isConnect) {
            // Prevent creating a new connection if already connected
            if (this.debug) {
                console.log('Create connection ignored - connected');
            }
            return;
        }

        this.client = new net.Socket();

        if (this.debug) {
            console.log('Connecting ....');
        }
        this.client.connect(this.port, this.url, () => {
            if (this.debug) {
                console.log('Connected to server:', this.url);
            }
            this.isConnect = true;
            if (this.connectListener) {
                this.connectListener();
            }
        });

        this.client.on('data', (data) => {
            this.handleData(data);
        });

        this.client.on('close', () => {
            if (this.debug) {
                console.log('Connection closed - on close event');
            }
            this.isConnect = false;
            if (this.disconnectListener) {
                this.disconnectListener();
            }
            if (!this.reconnecting) {
                this.attemptReconnect();
            }
        });

        this.client.on('error', (err) => {
            if (this.debug) {
                console.error('Socket error:', err);
            }
            this.client.destroy();
            this.isConnect = false;
            if (!this.reconnecting) {
                this.attemptReconnect(err);
            }
        });
    }

    attemptReconnect(err = null) {
        if (this.reconnecting) {
            if (this.debug) {
                console.error('Prevent nested reconnects');
            }
            return;
        }

        if (!this.allowReconnect) {
            if (this.debug) {
                console.error('Reconnect disabled');
            }
            return;
        }

        this.reconnecting = true;

        if (this.reconnectStartListener) {
            this.reconnectStartListener();
        }

        if (this.debug) {
            console.error('Set reconnect backoff timer following error:', err.message);
        }
        setTimeout(() => {
            try {
                if (this.debug) {
                    console.log('Attempting to reconnect...');
                }
                this.createConnection();
            } catch (error) {
                if (this.debug) {
                    console.error('Reconnection attempt failed:', error);
                }
            } finally {
                this.reconnecting = false;
            }
        }, this.reconnectInterval);
    }

    isConnected() {
        return this.isConnect;
    }

    closeConnection() {
        if (this.client) {
            this.client.end();
            this.client.destroy();
            this.isConnect = false;
            this.reconnecting = false; // Ensure reconnection is stopped
            this.allowReconnect = false;
        }
    }

    send(data) {
        if (this.isConnect && this.client) {
            if (this.debug) {
                console.log('Send data: ', data);
            }
            this.client.write(data);
        } else {
            if (this.debug) {
                console.error('Attempted to send data on a closed connection.');
            }
        }
    }

    handleData(data) {
        if (this.debug) {
            console.log('Receive raw data length: ', data.length);
        }
        this.buffer = Buffer.concat([this.buffer, data]);
        let endIndex = this.buffer.indexOf(this.END_CHARACTER);
        while (endIndex !== -1) {
            const packet = this.buffer.slice(0, endIndex + 1);
            if (this.receivedDataHandler) {
                if (this.debug) {
                    console.log('Receive data handler length: ', data.length);
                }
                this.receivedDataHandler(packet);
            }
            this.buffer = this.buffer.slice(endIndex + 1);
            endIndex = this.buffer.indexOf(this.END_CHARACTER);
        }
    }
}

module.exports = Machine;
