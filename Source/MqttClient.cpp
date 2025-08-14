#include "MqttClient.h"

//==============================================================================
MqttClient::MqttClient() : juce::Thread("MqttClient"), client(nullptr)
{
    DBG("MqttClient created (Eclipse Paho C implementation)");
}

MqttClient::~MqttClient()
{
    disconnect();
    stopThread(5000); // Wait up to 5 seconds for thread to stop

    if (client)
    {
        MQTTAsync_destroy(&client);
    }
}

//==============================================================================
void MqttClient::connect(const juce::String &brokerUrl,
                         const juce::String &clientId,
                         const juce::String &username,
                         const juce::String &password)
{
    this->brokerUrl = brokerUrl;
    this->clientId = clientId.isEmpty() ? "KadmiumDMX_" + juce::Uuid().toString() : clientId;
    this->username = username;
    this->password = password;

    shouldConnect = true;

    DBG("MQTT Connect requested to: " + brokerUrl);

    if (!isThreadRunning())
    {
        startThread();
    }
}

void MqttClient::disconnect()
{
    shouldConnect = false;

    if (client && isConnected.load())
    {
        MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
        opts.timeout = 10000;
        MQTTAsync_disconnect(client, &opts);
        isConnected = false;
    }

    DBG("MQTT Disconnect requested");
}

void MqttClient::subscribe(const juce::String &topic)
{
    if (!isConnected.load() || !client)
    {
        DBG("MQTT not connected, cannot subscribe to: " + topic);
        return;
    }

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    int rc = MQTTAsync_subscribe(client, topic.toRawUTF8(), 1, &opts);

    if (rc == MQTTASYNC_SUCCESS)
    {
        DBG("MQTT subscribed to: " + topic);
        juce::ScopedLock lock(subscriptionsMutex);
        subscribedTopics.add(topic);
    }
    else
    {
        DBG("MQTT subscribe error: " + juce::String(rc));
    }
}

void MqttClient::unsubscribe(const juce::String &topic)
{
    if (!isConnected.load() || !client)
        return;

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    int rc = MQTTAsync_unsubscribe(client, topic.toRawUTF8(), &opts);

    if (rc == MQTTASYNC_SUCCESS)
    {
        DBG("MQTT unsubscribed from: " + topic);
        juce::ScopedLock lock(subscriptionsMutex);
        subscribedTopics.removeString(topic);
    }
    else
    {
        DBG("MQTT unsubscribe error: " + juce::String(rc));
    }
}

void MqttClient::publish(const juce::String &topic, const juce::String &message, int qos, bool retain)
{
    if (!isConnected.load() || !client)
    {
        DBG("MQTT not connected, cannot publish to: " + topic);
        return;
    }

    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

    auto messageBytes = message.toRawUTF8();
    pubmsg.payload = (void *)messageBytes;
    pubmsg.payloadlen = (int)strlen(messageBytes);
    pubmsg.qos = qos;
    pubmsg.retained = retain ? 1 : 0;

    int rc = MQTTAsync_sendMessage(client, topic.toRawUTF8(), &pubmsg, &opts);

    if (rc == MQTTASYNC_SUCCESS)
    {
        DBG("MQTT published to '" + topic + "': " + message);
    }
    else
    {
        DBG("MQTT publish error: " + juce::String(rc));
    }
}

//==============================================================================
void MqttClient::setConnectionCallback(ConnectionCallback callback)
{
    connectionCallback = callback;
}

void MqttClient::setMessageCallback(MessageCallback callback)
{
    messageCallback = callback;
}

juce::StringArray MqttClient::getSubscribedTopics() const
{
    juce::ScopedLock lock(subscriptionsMutex);
    return subscribedTopics;
}

//==============================================================================
void MqttClient::run()
{
    DBG("MQTT Client thread started (Eclipse Paho C implementation)");

    while (!threadShouldExit())
    {
        if (shouldConnect.load() && !isConnected.load())
        {
            attemptConnection();
        }

        // Check connection status every second
        wait(1000);
    }

    DBG("MQTT Client thread stopped");
}

void MqttClient::attemptConnection()
{
    if (client)
    {
        MQTTAsync_destroy(&client);
    }

    int rc = MQTTAsync_create(&client, brokerUrl.toRawUTF8(), clientId.toRawUTF8(), MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (rc != MQTTASYNC_SUCCESS)
    {
        DBG("MQTT Client creation failed: " + juce::String(rc));
        handleConnectionResult(false, "Client creation failed: " + juce::String(rc));
        return;
    }

    // Set callbacks
    MQTTAsync_setCallbacks(client, this, onConnectionLost, onMessageArrived, onDeliveryComplete);

    // Connection options
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.onSuccess = onConnectSuccess;
    conn_opts.onFailure = onConnectFailure;
    conn_opts.context = this;

    if (!username.isEmpty())
    {
        conn_opts.username = username.toRawUTF8();
        if (!password.isEmpty())
        {
            conn_opts.password = password.toRawUTF8();
        }
    }

    DBG("MQTT attempting connection to: " + brokerUrl);
    rc = MQTTAsync_connect(client, &conn_opts);
    if (rc != MQTTASYNC_SUCCESS)
    {
        DBG("MQTT connection attempt failed: " + juce::String(rc));
        handleConnectionResult(false, "Connection attempt failed: " + juce::String(rc));
    }
}

//==============================================================================
// Static callback functions
void MqttClient::onConnectionLost(void *context, char *cause)
{
    auto *mqttClient = static_cast<MqttClient *>(context);
    if (mqttClient)
    {
        juce::String causeStr = cause ? juce::String(cause) : "Unknown reason";
        DBG("MQTT connection lost: " + causeStr);
        mqttClient->isConnected = false;
        mqttClient->handleConnectionResult(false, "Connection lost: " + causeStr);
    }
}

int MqttClient::onMessageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    auto *mqttClient = static_cast<MqttClient *>(context);
    if (mqttClient && topicName && message)
    {
        juce::String topic(topicName, topicLen > 0 ? topicLen : strlen(topicName));
        juce::String msg(static_cast<char *>(message->payload), message->payloadlen);

        DBG("MQTT message received on '" + topic + "': " + msg);
        mqttClient->handleMessage(topic, msg);

        MQTTAsync_freeMessage(&message);
        MQTTAsync_free(topicName);
    }
    return 1; // Message processed successfully
}

void MqttClient::onDeliveryComplete(void *context, MQTTAsync_token token)
{
    DBG("MQTT message delivery complete (token: " + juce::String(token) + ")");
}

void MqttClient::onConnectSuccess(void *context, MQTTAsync_successData *response)
{
    auto *mqttClient = static_cast<MqttClient *>(context);
    if (mqttClient)
    {
        DBG("MQTT connection successful");
        mqttClient->isConnected = true;
        mqttClient->handleConnectionResult(true);
    }
}

void MqttClient::onConnectFailure(void *context, MQTTAsync_failureData *response)
{
    auto *mqttClient = static_cast<MqttClient *>(context);
    if (mqttClient)
    {
        juce::String error = response ? "Error code: " + juce::String(response->code) : "Unknown error";
        DBG("MQTT connection failed: " + error);
        mqttClient->handleConnectionResult(false, error);
    }
}

//==============================================================================
void MqttClient::handleConnectionResult(bool success, const juce::String &error)
{
    if (connectionCallback)
    {
        connectionCallback(success, error);
    }
}

void MqttClient::handleMessage(const juce::String &topic, const juce::String &message)
{
    if (messageCallback)
    {
        messageCallback(topic, message);
    }
}
