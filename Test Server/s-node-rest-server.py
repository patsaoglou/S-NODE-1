from flask import Flask, jsonify, request, render_template
from flask_sqlalchemy import SQLAlchemy
from sqlalchemy.exc import SQLAlchemyError
import uuid
from datetime import datetime

app = Flask(__name__)
sleep_time_minutes = 1
app.config['SQLALCHEMY_DATABASE_URI'] = 'mysql+mysqlconnector://root:0000@localhost/s-node-1'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

db = SQLAlchemy(app)

class Device(db.Model):
    __tablename__ = 'device'
    id = db.Column(db.Integer, primary_key=True)
    uuid = db.Column(db.String(36), unique=True, nullable=False, primary_key=True)

    def __repr__(self):
        return f'<Device {self.uuid}>'

class Measurement(db.Model):
    __tablename__ = 'measurement'
    id = db.Column(db.Integer, primary_key=True)
    temp = db.Column(db.Float, nullable=False)
    humid = db.Column(db.Float, nullable=False)
    pressure = db.Column(db.Float, nullable=False)
    battery_voltage = db.Column(db.Float, nullable=False)
    solar_voltage = db.Column(db.Float, nullable=False)
    solar_current = db.Column(db.Float, nullable=False)
    wifi_signal_strength = db.Column(db.Float, nullable=False)
    device_id = db.Column(db.String(36), db.ForeignKey('device.uuid'), nullable=False)
    timestamp = db.Column(db.DateTime, default=datetime.now)
    device = db.relationship('Device', back_populates='measurements')

    def __repr__(self):
        return f'<Measurement {self.id}>'

Device.measurements = db.relationship('Measurement', order_by=Measurement.id, back_populates='device')

with app.app_context():
    try:
        db.create_all()
        print("Database tables created.")
    except SQLAlchemyError as e:
        print(f"Error creating tables: {e}")

@app.route('/device/1/last_measurement_json', methods=['GET'])
def get_last_measurement_json():
    device = Device.query.filter_by(id=1).first()
    
    if not device:
        return jsonify({"error": "Device not found"}), 404

    last_measurement = Measurement.query.filter_by(device_id=device.uuid).order_by(Measurement.timestamp.desc()).first()

    if not last_measurement:
        return jsonify({"error": "No measurements found for this device"}), 404

    return jsonify({
        "temp": last_measurement.temp,
        "humid": last_measurement.humid,
        "pressure": last_measurement.pressure,
        "battery_voltage": last_measurement.battery_voltage,
        "solar_voltage": last_measurement.solar_voltage,
        "solar_current": last_measurement.solar_current,
        "wifi_signal_strength": last_measurement.wifi_signal_strength,
        "timestamp": last_measurement.timestamp
    })

@app.route('/device/1/last_measurement', methods=['GET'])
def display_last_measurement():
    return render_template('measurement.html')


@app.route('/add_device', methods=['POST'])
def add_device():
    new_uuid = str(uuid.uuid4())
    new_device = Device(uuid=new_uuid)
    
    try:
        db.session.add(new_device)
        db.session.commit()
    except SQLAlchemyError as e:
        db.session.rollback()
        print(f"Error adding device: {e}")
        return jsonify({"error": "Failed to add device"}), 500

    return jsonify({"uuid": new_uuid}), 201

@app.route('/receive_measurements', methods=['POST'])
def receive_measurements():
    global sleep_time_minutes
    measurements = request.json
    device_key = measurements.get('device_key')
    print(measurements)
    
    if not device_key:
        return jsonify({"error": "Device UUID is required"}), 400

    device = Device.query.filter_by(uuid=device_key).first()
    
    if not device:
        return jsonify({"error": "Device not found"}), 404

    new_measurement = Measurement(
        temp=measurements['temp'],
        humid=measurements['humid'],
        pressure=measurements['pressure'],
        battery_voltage=measurements['battery_voltage'],
        solar_voltage=measurements['solar_voltage'],
        solar_current=measurements['solar_current'],
        wifi_signal_strength=measurements['wifi_signal_strength'],
        device=device
    )
    
    try:
        db.session.add(new_measurement)
        db.session.commit()
    except SQLAlchemyError as e:
        db.session.rollback()
        print(f"Error saving measurement: {e}")
        return jsonify({"error": "Failed to save measurement"}), 500

    sleep_time_minutes = 1

    response_data = {
        "sleep_duration": sleep_time_minutes
    }
    print(response_data)
    return jsonify(response_data)

if __name__ == '__main__':
    app.run(host='0.0.0.0', debug=True, port=5000)
