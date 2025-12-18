from flask import Flask, render_template, jsonify, request
import numpy as np
import joblib
import os

app = Flask(__name__)

# Attempt to load the pre-trained AI model
try:
    if os.path.exists('theft_model.pkl'):
       # print("Pre-trained AI model 'theft_model.pkl' loaded.")
        ai_model = joblib.load('theft_model.pkl')
        # Note: The 'ai_model' variable is loaded but not currently used in the predict_theft function.
    else:
       # print("Warning: AI model file 'theft_model.pkl' not found.")
        ai_model = None
except Exception as e:
    print(f"Error loading AI model: {e}.")
    ai_model = None

def predict_theft(data):
    """
    Analyzes current data to predict theft.
    Note: This function currently uses a hardcoded rule-based system
    and does not use the loaded 'ai_model' variable.
    """
    try:
        pole_current = data.get('pole', 0)
        total_current = data.get('total', 0)
        difference = abs(pole_current - total_current)

        # Rule-based logic for theft detection
        if difference > 0.5 and pole_current > 0.3:
            prediction = "THEFT DETECTED!"
            # Calculate confidence for theft
            confidence = min(99.0, 95 + (difference - 0.5) * 5)
        else:
            prediction = "Normal"
            # Calculate confidence for normal operation
            confidence = min(99.9, 98 + np.random.rand())

        return {"prediction": prediction, "confidence": round(confidence, 2)}
    
    except Exception as e:
        print(f"Error during prediction: {e}")
        return {"prediction": "Error", "confidence": 0.0}

@app.route('/')
def index():
    # Renders the main dashboard page
    return render_template('dashboard.html')

@app.route('/api/predict', methods=['POST'])
def api_predict():
    # API endpoint to get a prediction
    data = request.json
    result = predict_theft(data)
    return jsonify(result)

if __name__ == '__main__':
    # Runs the Flask application
    app.run(host='0.0.0.0', port=5001, debug=True)
