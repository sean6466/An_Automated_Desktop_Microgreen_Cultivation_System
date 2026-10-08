import requests
import csv
import os
import time
from datetime import datetime

# ============================================================
# SETTINGS
# ============================================================

# Change this IP address if your Arduino's IP changes
DEVICE_IP = "192.168.1.105"

# JSON endpoint
URL = f"http://{DEVICE_IP}/"

# Recording interval in seconds
RECORD_INTERVAL = 60

# Output CSV file
CSV_FILE = "microgreen_data.csv"

# ============================================================
# CSV SETUP
# ============================================================

HEADERS = [
    "timestamp",
    "co2",
    "temp",
    "hum",
    "pressure",
    "visible",
    "ir",
    "soil",
    "Relay1",
    "Relay2",
    "Relay3",
    "Relay4",
]


def create_csv_if_needed():
    """Create the CSV file and headers if it does not exist."""

    if not os.path.exists(CSV_FILE):
        with open(CSV_FILE, mode="w", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=HEADERS)
            writer.writeheader()

        print(f"Created new file: {CSV_FILE}")


def get_sensor_data():
    """Request JSON data from the Arduino."""

    try:
        response = requests.get(URL, timeout=10)

        # Check for HTTP errors
        response.raise_for_status()

        # Convert JSON response into Python dictionary
        data = response.json()

        return data

    except requests.exceptions.ConnectionError:
        print("ERROR: Could not connect to the device.")
        print(f"Check that the device is available at: {URL}")

    except requests.exceptions.Timeout:
        print("ERROR: Connection timed out.")

    except requests.exceptions.JSONDecodeError:
        print("ERROR: Device did not return valid JSON.")
        print("Response received:")
        print(response.text)

    except requests.exceptions.RequestException as e:
        print(f"ERROR: {e}")

    return None

def save_data(data):
    """Add timestamp and save sensor data and relay states to CSV."""

    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    # Get relay states from Arduino JSON
    relays = data.get("relays", [False, False, False, False])

    row = {
        "timestamp": timestamp,
        "co2": data.get("co2", ""),
        "temp": data.get("temp", ""),
        "hum": data.get("hum", ""),
        "pressure": data.get("pressure", ""),
        "visible": data.get("visible", ""),
        "ir": data.get("ir", ""),
        "soil": data.get("soil", ""),

        # Relay states
        "relay1": relays[0] if len(relays) > 0 else "",
        "relay2": relays[1] if len(relays) > 1 else "",
        "relay3": relays[2] if len(relays) > 2 else "",
        "relay4": relays[3] if len(relays) > 3 else ""
    }

    # Save row to CSV
    with open(CSV_FILE, "a", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=row.keys())

        # Write header if the file is empty
        if file.tell() == 0:
            writer.writeheader()

        writer.writerow(row)

    print(
        f"[{timestamp}] "
        f"CO2: {row['co2']} | "
        f"Temp: {row['temp']} | "
        f"Humidity: {row['hum']} | "
        f"Soil: {row['soil']} |"
        f"Pressure: {row['pressure']} |"
        f"Visible: {row['visible']} |"
        f"IR: {row['ir']} "
    )


def main():

    print("======================================")
    print(" Microgreen Data Logger")
    print("======================================")
    print(f"Device URL: {URL}")
    print(f"Recording interval: {RECORD_INTERVAL} seconds")
    print(f"Output file: {CSV_FILE}")
    print("Press Ctrl + C to stop.")
    print("======================================")

    create_csv_if_needed()

    while True:

        print("\nRequesting sensor data...")

        data = get_sensor_data()

        if data is not None:
            save_data(data)

        print(f"Waiting {RECORD_INTERVAL} seconds...")
        time.sleep(RECORD_INTERVAL)


if __name__ == "__main__":
    try:
        main()

    except KeyboardInterrupt:
        print("\n\nData logger stopped.")