import { useEffect, useState } from "react"; //Imports useEffect and useState from react
//In react useState is used to store values that change whilst the app is running
//In react useEffect is used to fetch the values at regular intervals whilst the app is running
import { Line } from "react-chartjs-2"; //Imports line graph from react-chartjs-2
import "chart.js/auto"; //Automatically imports the chart js components required for the line graph (graph for sensor values)

const ARDUINO_IP = "/api";

export default function App() { //Sets the main function for the app
  //Sensor chart state variables
  const [data, setData] = useState(null); //Declares a state variable data with the initial value as null. setData is the function used to update the state of data
  const [history, setHistory] = useState([]); //Declares history as a state variable that contains the chart history array
  const [selectedSensor, setSelectedSensor] = useState("temp"); //Declares the selected sensor as a state variable. Sets the default display sensor to temperature

  //Relay states
  const [relays, setRelays] = useState([false, false, false, false]); //Declares relays as a state variable array. Sets the default relay state to false (off)

  //Control mode
  const [autoMode, setAutoMode] = useState(true);

  //Setpoints
  const [setpoints, setSetpoints] = useState({

    temp_min: 18,
    temp_max: 25,
    temp_db: 1,

    hum_min: 70,
    hum_max: 100,
    hum_db: 3,

    moist_min: 70,
    moist_max: 100,
    moist_db: 3,
  });

//Fetch Sensor Data
useEffect(() => {
  const interval = setInterval(async () => {
    try {
      const res = await fetch(ARDUINO_IP); //Fetch request to the Arduino IP address
      const json = await res.json(); //Fetches the JSON string from the Arduino address
      setData(json);
      // Update relay states if supplied by Arduino
      if (json.relays) {
        setRelays(json.relays); //Updates the setRelays with the relay states from the Arduino 
      }
      // Update graph history
      setHistory(prev => { //Updates the set history array
        const updated = [...prev, json[selectedSensor]]; //Uses the JSON data for the selected sensor
        if (updated.length > 40) {
          updated.shift();
        }
        return updated;
      });
    }
    catch (e) {console.error("Fetch error", e);}//Print error to the browser console
  }, 2000);
  return () => clearInterval(interval); //Clears the interval
}, [selectedSensor]); //Returns the selected sensor

//No Arduino connection
if (!data) return <h2 style={{ color: "white" }}>Connecting...</h2>; //Error handling, if no data availible then display Connecting...

const chartData = { //Defines the chart data array
  labels: history.map((_, i) => i), //Increments the x axis by one for each datapoint in history
  datasets: [
    {
      label: selectedSensor, //Label is the selected sensor
      data: history, //Data is defined as history
      borderColor: "#4d99e1", //Sets the colour
    },
  ],
};

//Control mode change function
const changeMode = async () => {
  const newMode = !autoMode;
  try {
    await fetch(
    `${ARDUINO_IP}/mode?mode=${newMode ? "auto" : "manual"}`
    );
    setAutoMode(newMode);
  }
  catch (e) {console.error("Control mode error", e);} //Error handling
};

//Relay control function
const toggleRelay = async (index) => {
  if (autoMode) { //If the toggle is in auto mode, block relay control
    console.log("Relay control disabled in Auto mode"); //Displays relay disabled message
    return;
  }

  const newState = !relays[index];
    try {
      await fetch(
        `${ARDUINO_IP}/relay?ch=${index + 1}&state=${newState ? "on" : "off"}` //Checks the relay state from the Arduino and updates
      );
      const updated = [...relays];
      updated[index] = newState;
      setRelays(updated); //Sets the toggle to the updated state 
    } catch (e) {console.error("Relay error", e);} //Error handling
};


//Setpoint update function
const updateSetpoint = (name, value) => { //Takes the setpoint name and value as input
  setSetpoints(prev => ({
    ...prev,
    [name]: Number(value) //Returns name and value
  }));
};

//Send updated setpoints to Arduino
const applySetpoints = async () => {
  try {
    const params = new URLSearchParams(setpoints); //Defines the setpoints as the parameters for the URL string (JSON)
    await fetch(
    `${ARDUINO_IP}/setpoints?${params.toString()}` //Sends the params data to the Arduino
   );
    console.log("Setpoints updated"); //Logs setpoint update to the console
    alert("Setpoints successfully sent to Arduino"); //Creates a popup dialog confirming setpoints have been sent to the Arduino
  }
  catch (e) { //Standard error handling
    console.error("Setpoint error", e);
    alert("Failed to send setpoints"); //Error popup dialogue
  }
};

//HTML function, function is HTML so commenting every line is not practical. The function sets the styles (defined below) for the chart, the toggle switch for Auto/Manual mode, and the sensor data cards
return (
  <div style={styles.container}>
    {/* TITLE */}
    <h1 style={styles.title}>
      Environment Dashboard
      </h1>

    {/* AUTO / MANUAL MODE */}
    <div style={styles.modePanel}>
      <h2>Control Mode</h2>
      <div style={styles.modeRow}>
        <span style={styles.modeText}>
          {autoMode ? "Auto Mode" : "Manual Mode"}
        </span>

        <div
          style={{
            ...styles.modeToggle,
            background: autoMode
              ? "#34c759"
              : "#ff9500",
          }}
          onClick={changeMode}
        >

          <div
            style={{
              ...styles.modeToggleKnob,
              transform: autoMode
                ? "translateX(30px)"
                : "translateX(3px)",
            }}
          />

        </div>

      </div>
      <p style={styles.modeDescription}>
        {autoMode
          ? "Automatic control active. Relay controls are disabled."
          : "Manual control active. Relays can be controlled from the dashboard."
        }

      </p>
    </div>

    {/* Sensor Cards */}
    <div style={styles.grid}>
      <Card
        title="CO₂"
        value={data.co2 + " ppm"}
      />

      <Card
        title="Temperature"
        value={data.temp.toFixed(1) + " °C"}
      />

      <Card
        title="Humidity"
        value={data.hum.toFixed(1) + " %"}
      />

      <Card 
        title="Pressure"
        value={data.pressure.toFixed(1) + " hPa"}
      />

      <Card
        title="Light"
        value={`${data.visible}/${data.ir}`}
      />

      <Card
        title="Soil Moisture"
        value={data.soil.toFixed(1) + " %"}
      />

      <Card
        title="Water Level Alarm"
        value={data.waterlow ? "Water low" : "Ok"}
      />

    </div>

    {/* Sensor Selection */}
    <select
      value={selectedSensor}
      onChange={(e) => {
        setSelectedSensor(e.target.value);
        setHistory([]);
      }}

      style={styles.dropdown}
    >

      <option value="co2">CO₂</option>
      <option value="temp">Temperature</option>
      <option value="hum">Humidity</option>
      <option value="pressure">Pressure</option>
      <option value="visible">Visible Light</option>
      <option value="ir">IR Light</option>
      <option value="soil">Soil Moisture</option>

    </select>

    {/* Graph */}
    <div style={styles.chart}>
      <Line data={chartData} />
    </div>

    {/* Setpoints */}
    <div style={styles.setpoints}>
      <h2>Control Setpoints</h2>
      {/* Temperature */}
      <h3>Temperature (°C)</h3>
      <div style={styles.inputGrid}>
         <SetpointInput
          label="Minimum"
          value={setpoints.temp_min}
          onChange={(value) =>
            updateSetpoint("temp_min", value)
          }
          />

        <SetpointInput
          label="Maximum"
          value={setpoints.temp_max}
          onChange={(value) =>
            updateSetpoint("temp_max", value)
          }
        />

        <SetpointInput
          label="Deadband"
          value={setpoints.temp_db}
          onChange={(value) =>
            updateSetpoint("temp_db", value)
          }
        />

      </div>

      {/* Humidity */}
      <h3>Humidity (%)</h3>
       <div style={styles.inputGrid}>
        <SetpointInput
          label="Minimum"
          value={setpoints.hum_min}
          onChange={(value) =>
            updateSetpoint("hum_min", value)
          }
        />

        <SetpointInput
          label="Maximum"
          value={setpoints.hum_max}
          onChange={(value) =>
            updateSetpoint("hum_max", value)
          }
        />

        <SetpointInput
          label="Deadband"
          value={setpoints.hum_db}
          onChange={(value) =>
            updateSetpoint("hum_db", value)
          }
        />

      </div>

      {/* Soil Moisture */}
      <h3>Soil Moisture (%)</h3>
      <div style={styles.inputGrid}>
        <SetpointInput
          label="Minimum"
          value={setpoints.moist_min}
          onChange={(value) =>
            updateSetpoint("moist_min", value)
          }
        />

        <SetpointInput
          label="Maximum"
          value={setpoints.moist_max}
          onChange={(value) =>
            updateSetpoint("moist_max", value)
          }
        />

        <SetpointInput
          label="Deadband"
          value={setpoints.moist_db}
          onChange={(value) =>
            updateSetpoint("moist_db", value)
          }
        />
      </div>
      <button
        style={styles.applyButton}
        onClick={applySetpoints}
      >

        Apply Setpoints
       </button>
      </div>


    {/* Relays */}
    <div style={styles.relays}>
      <h2>
        Relays
        {autoMode && (
          <span style={styles.autoLabel}>
            AUTO CONTROL ACTIVE
          </span>
        )}

      </h2>
      {relays.map((state, i) => (
        <div
          key={i}
          style={{
            ...styles.relayRow,
            opacity: autoMode ? 0.5 : 1,
          }}
        >
          <span>
            Relay {i + 1}
          </span>

          <div style={styles.toggleContainer}>
            <span style={styles.toggleLabel}>
              {state ? "On" : "Off"}
            </span>

            <div
              style={{
                ...styles.toggle,
                background: state
                  ? "#34c759"
                  : "#555",

                cursor: autoMode
                  ? "not-allowed"
                  : "pointer",

              }}

            onClick={() => toggleRelay(i)}
            >
              <div
                style={{
                  ...styles.toggleKnob,
                  transform: state
                    ? "translateX(26px)"
                    : "translateX(2px)",
                 }}
               />
              </div>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

// Sensor Card
function Card({ title, value }) { //Card function that takes sensor value and title and defines it using the card style
  return (
    <div style={styles.card}>
      <h3>{title}</h3>
      <p>{value}</p>
    </div>
  );
}

// Setpoint Input
function SetpointInput({ label, value, onChange }) {//setpoint input function that takes label, user input value, and on state change function, and defines it using the input container style
  return (
    <div style={styles.inputContainer}>
      <label style={styles.inputLabel}>
        {label}
      </label>

      <input
        type="number"
        value={value}
        onChange={(e) =>
          onChange(e.target.value)
        }
        style={styles.input}
      />
    </div>
  );
}

//Styles. This section defines the styles for each element used in the html page.
const styles = {
  container: { //container style
    fontFamily: "Arial",
    background: "#0b0f19",
    color: "white",
    minHeight: "100vh",
    padding: "20px",
  },

  title: { //Title style
    color: "white",
  },

  grid: { //Grid style
    display: "grid",
    gridTemplateColumns:
      "repeat(auto-fit, minmax(150px, 1fr))",
    gap: "10px",
  },

  card: { //Card style
    background: "#1c2333",
    padding: "15px",
    borderRadius: "10px",
    textAlign: "center",
  },

  dropdown: { //Dropdown style
    marginTop: "20px",
    padding: "10px",
    borderRadius: "8px",
    background: "#1c2333",
    color: "white",
    border: "none",
  },

  chart: { //Chart style
    marginTop: "20px",
    background: "#1c2333",
    padding: "10px",
    borderRadius: "10px",
  },

  // Mode Header
  modePanel: {
    marginTop: "20px",
    background: "#1c2333",
    padding: "15px",
    borderRadius: "10px",
    marginBottom: "20px",
  },

  modeRow: {
    display: "flex",
    justifyContent: "space-between",
    alignItems: "center",
  },

  modeText: {
    fontSize: "18px",
    fontWeight: "bold",
    color: "#aaa",
  },

  modeDescription: {
    color: "#aaa",
  },

  modeToggle: { //Toggle switch body mode
    width: "60px",
    height: "30px",
    borderRadius: "20px",
    position: "relative",
    cursor: "pointer",
    transition: "0.3s",
  },

  modeToggleKnob: { //Toggle knob mode
    width: "24px",
    height: "24px",
    background: "white",
    borderRadius: "50%",
    position: "absolute",
    top: "3px",
    left: "0",
    transition: "0.3s",
  },

  // Setpoints
  setpoints: { 
    marginTop: "20px",
    background: "#1c2333",
    padding: "15px",
    borderRadius: "10px",
  },

  inputGrid: {
    display: "grid",
    gridTemplateColumns:
      "repeat(auto-fit, minmax(120px, 1fr))",
    gap: "10px",
    marginBottom: "20px",
  },

  inputContainer: {
    display: "flex",
    flexDirection: "column",
    gap: "5px",
  },

  inputLabel: {
    color: "#aaa",
  },

  input: {
    padding: "10px",
    borderRadius: "6px",
    border: "none",
    background: "#0b0f19",
    color: "white",
    fontSize: "16px",
  },

  applyButton: {
    padding: "12px 20px",
    borderRadius: "8px",
    border: "none",
    background: "#4dd0e1",
    color: "#000",
    fontWeight: "bold",
    cursor: "pointer",
    fontSize: "16px",
  },

  // Relays
  relays: {
    marginTop: "20px",
    background: "#1c2333",
    padding: "15px",
    borderRadius: "10px",
  },

  relayRow: {
    display: "flex",
    justifyContent: "space-between",
    marginTop: "10px",
    alignItems: "center",
    transition: "0.3s",
  },

  toggle: {
    width: "50px",
    height: "26px",
    borderRadius: "20px",
    position: "relative",
  },

  toggleKnob: {
    width: "22px",
    height: "22px",
    background: "white",
    borderRadius: "50%",
    position: "absolute",
    top: "2px",
    left: "0",
    transition: "0.3s",
  },

  toggleContainer: {
    display: "flex",
    alignItems: "center",
    gap: "10px",
  },

  toggleLabel: {
    color: "#ccc",
  },

  autoLabel: {
    marginLeft: "15px",
    fontSize: "12px",
    color: "#34c759",
  },
};