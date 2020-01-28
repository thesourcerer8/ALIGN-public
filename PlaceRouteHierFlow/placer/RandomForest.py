from sklearn.ensemble import RandomForestRegressor
import numpy as np
from sklearn.externals import joblib
import matplotlib.pyplot as plt
from sklearn.metrics import mean_squared_error, r2_score


def load_data(fname):
    data_set = np.load(fname)
    return data_set['x_rc_train'], data_set['x_rc_test'], data_set['y_train'], data_set['y_test'], data_set['X_rc_name'], data_set['Y_name']


def load_data_device(fname):
    data_set = np.load(fname)
    return data_set['x_size_train'], data_set['x_size_test'], data_set['X_size_name']


def train_valid_split(x_train, y_train, split_index=400):
    x_train_new = x_train[:split_index]
    y_train_new = y_train[:split_index]
    x_valid = x_train[split_index:]
    y_valid = y_train[split_index:]
    return x_train_new, y_train_new, x_valid, y_valid


def load_model(save_path):
    model = joblib.load(save_path)
    return model


def save_model(model, save_path):
    joblib.dump(model, save_path)


def relative_error(y_test, y_predict):
    y_test = np.array(y_test)
    y_predict = np.array(y_predict)
    sum = 0
    for i in range(len(y_predict)):
        sum = sum + abs(y_test[i] - y_predict[i]) / abs(y_test[i])
    sum = sum / len(y_predict)
    return sum


def plot_result(y_test, y_predict, feature_name):
    fig = plt.figure()
    ax = fig.add_subplot(111)
    ax.plot(y_test, y_predict, '.')
    min_x = min(y_test)
    max_x = max(y_test)
    min_x_predict = min(y_predict)
    max_x_predict = max(y_predict)
    minx = min(min_x, min_x_predict)
    maxx = max(max_x, max_x_predict)
    linex = [minx, maxx]
    liney = linex
    ax.plot(linex, liney, '-')
    ax.set_xlabel('Ture value each sample')
    ax.set_ylabel('Predicted value each sample')
    testing_rmse = np.sqrt(mean_squared_error(y_test, y_predict))
    r_score = r2_score(y_test, y_predict)
    relative_error_sample = relative_error(y_test, y_predict)
    print(feature_name + ' rscore', r_score)
    print(feature_name + ' relative_error_sample', relative_error_sample)
    ax.set_title(' %s, RMSE = %f' % (feature_name, testing_rmse))
    plt.show()


def train_models(x_train, y_train, x_test, y_test, feature_name, hparam, save=True, plot=True, save_path="model/"):
    models = []
    for i in range(4):
        # four models trained separately
        model = RandomForestRegressor(n_estimators=hparam["n_estimators"], max_depth=hparam["max_depth"], random_state=0)
        model.fit(x_train, y_train[:, i])
        models.append(model)
        if save:
            save_model(model, save_path + "RandomForest_" + feature_name[i] + ".m")
    if plot:
        for i in range(4):
            y_predict = models[i].predict(x_test)
            plot_result(y_test[:, i], y_predict, feature_name=feature_name[i])
    return models


def predict_model(model_path, x_test):
    if (isinstance(x_test, list) or len(np.array(x_test).shape) == 1):
        x_test = np.array(x_test).reshape(1, -1)
    #x_test = np.array(x_test).reshape(1, -1)
    model = load_model(model_path)
    y_predict = model.predict(x_test)
    return y_predict


def predict_models(model_path, x_test, y_test, feature_name, plot=False):
    for i in range(4):
        y_predict = predict_model(model_path + "RandomForest_" + feature_name[i] + ".m", x_test)
        if plot:
            plot_result(y_test[:, i], y_predict, feature_name=feature_name[i])


if __name__ == '__main__':
    data_file = "../../Data/cascode_current_mirror_ota_1_16_3.npz"
    x_train, x_test, y_train, y_test, x_name, y_name = load_data(data_file)
    hparam = {"n_estimators": 100, "criterion": "mse", "max_depth": 20}
    model_path = "model/"
    train_models(x_train, y_train, x_test, y_test, y_name, hparam, save=True, plot=True, save_path=model_path)
    predict_models(model_path, x_test, y_test, y_name, plot=False)
    #y_predict = predict_model(model_path + "RandomForest_" + y_name[0] + ".m", x_test)
