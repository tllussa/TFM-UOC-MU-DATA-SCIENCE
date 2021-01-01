from django.http import HttpResponse
from django.views.decorators.csrf import csrf_exempt
import urllib.request
import numpy as np
import json
import pandas as pd
from sklearn.preprocessing import StandardScaler
# Standardize/scale the dataset and apply PCA
from sklearn.decomposition import PCA
from sklearn.pipeline import make_pipeline
import gspread
import gspread_dataframe as gd

from oauth2client.service_account import ServiceAccountCredentials

# Import IsolationForest
from sklearn.ensemble import IsolationForest

import pickle

TIME_STEPS = 6

def load_model(filename):
    with open(filename, 'rb') as f:
        clf = pickle.load(f)
    return clf

# Create your views here.
@csrf_exempt
def isolationforest(request):
    received_json_data=json.loads(request.body)
    telemetry = received_json_data['telemetry']
    df = pd.json_normalize(telemetry)
    listTimes = df['Time']
    df['Time'] = pd.to_datetime(df['Time'])
    df['PM25'] = df['PM25'].astype(np.float32)
    df['PM25c'] = df['PM25'].astype(np.float32)
    df = df.set_index('Time')
    test = df.copy()

    #PCA
    test_names=test.columns
    x_test = test[test_names]

    scaler = StandardScaler()
    pca = PCA()
    pipeline = make_pipeline(scaler, pca)
    pipeline.fit(x_test)
    # Calculate PCA with 1 component
    pca = PCA(n_components=2)
    principalComponents = pca.fit_transform(x_test)
    principalDf = pd.DataFrame(data = principalComponents, columns = ['pc1','pc2'])

    #MODEL
   
    model = load_model('Isolation.pkl')

    #predicció
    anomalies = pd.Series(model.predict(principalDf))
    print(anomalies)

    df['Anomaly'] = np.array(anomalies).astype(str).tolist()
    df['Anomaly'] = df['Anomaly'].replace('-1',True)
    df['Anomaly'] = df['Anomaly'].replace('1',False)
    df['Time'] = np.array(listTimes).tolist()
    df['Model']= np.array(['Isolation Forest', 'Isolation Forest', 'Isolation Forest','Isolation Forest', 'Isolation Forest', 'Isolation Forest'])
    df = df.drop(columns=['PM25c'])
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