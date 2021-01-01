from django.http import HttpResponse
from django.views.decorators.csrf import csrf_exempt
import urllib.request
import numpy as np
import json
import pandas as pd
from sklearn.preprocessing import StandardScaler
from tensorflow import keras
from keras import backend
import gspread
import gspread_dataframe as gd

from oauth2client.service_account import ServiceAccountCredentials

TIME_STEPS = 6

#crear seqüències
def create_sequences(X, y, time_steps=TIME_STEPS):
    return np.array(X), np.array(y)

#càlcul del threshold de test
def calculate_threshold(X_test, X_test_pred):
    distance = np.sqrt(np.mean(np.square(X_test_pred - X_test),axis=1))
    """Sorting the scores/diffs and using a 0.80 as cutoff value to pick the threshold"""
    distance.sort();
    cut_off = int(0.80 * len(distance));
    threshold = distance[cut_off];
    return threshold

def rmse(y_true, y_pred):
	return backend.sqrt(backend.mean(backend.square(y_pred - y_true), axis=-1))

# Create your views here.
@csrf_exempt
def neuralnetwork(request):
    received_json_data=json.loads(request.body)
    telemetry = received_json_data['telemetry']
    neuralModel = received_json_data['model']
    df = pd.json_normalize(telemetry)
    listTimes = df['Time']
    df['Time'] = pd.to_datetime(df['Time'])
    df['PM25'] = df['PM25'].astype(np.float32)
    df = df.set_index('Time')
    test = df.copy()
    #normalitzar les dades
    scaler = StandardScaler()
    test['PM25'] = scaler.fit_transform(test)
    print(test)
    #creem seqüencia amb finestra temporal per les dades de test
    X_test, y_test = create_sequences(test[['PM25']], test['PM25'])
    X_test = np.expand_dims(X_test, 2)
   
    print(f'Testing shape: {X_test.shape}')
    print(f'Testing shape: {y_test.shape}')

    modelName =""
    if neuralModel == 'LSTM':
            modelName = 'LSTM_1h_TFM.h5'
    elif neuralModel == 'GRU':
            modelName = 'GRU_1h_TFM.h5'
    elif neuralModel == 'DNN':
            modelName = 'DNN_1h_TFM.h5'
    
    model = keras.models.load_model(
                                    modelName,
                                    custom_objects={
                                        "rmse": rmse,
                                    }
                                    )

    #evaluem el model
    eval = model.evaluate(X_test, y_test)
    print("evaluate: ",eval)
    
    # #predim el model
    X_test_pred = model.predict(X_test, verbose=0)

    # #càlcul del rmse_loss
    test_rmse_loss = np.sqrt(np.mean(np.square(X_test_pred - X_test),axis=1))

    # # reshaping test prediction
    X_test_predReshape = X_test_pred.reshape((X_test_pred.shape[0] * X_test_pred.shape[1]), X_test_pred.shape[2])
    
    # # reshaping test data
    X_testReshape = X_test.reshape((X_test.shape[0] * X_test.shape[1]), X_test.shape[2])
  
    threshold = calculate_threshold(X_testReshape,X_test_predReshape)
    print("threshold",threshold)
    loss = test_rmse_loss.reshape((-1))
    print("loss",loss)
    anomalies = loss > threshold

    df['Anomaly'] = anomalies
    df['Time'] = np.array(listTimes).tolist()
    df['Model']= np.array([neuralModel, neuralModel, neuralModel,neuralModel,neuralModel,neuralModel])
    result = df.to_json(orient="records")
    parsed = json.loads(result)
    out = json.dumps(parsed)  

    # ACCES GOOGLE SHEET
    gc = gspread.service_account(filename='allussa-tfm-datascience-629a42e3a04c.json')
    sh = gc.open_by_key('1rABPVxIs0LBPbrX3txJQTIxDxaB1puRtSovBiZjnLLw')
    ws = sh.get_worksheet(0) #-> 0 - first sheet, 1 - second sheet etc. 
    ws.add_rows(df.shape[0])
    gd.set_with_dataframe(worksheet=ws,dataframe=df,resize=False,include_column_header=False,row=ws.row_count+1)
       
    return HttpResponse(out)