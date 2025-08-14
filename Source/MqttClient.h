#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <memory>
#include <MQTTAsync.h>

//==============================================================================
/**
 * MQTT Client wrapper for DMX light control using Eclipse Paho MQTT C library
 */

class MqttClient : public juce::Thread
{
public:
    // Connection status callback
    using ConnectionCallback = std::function<void(bool connected, const juce::String &error)>;

    // Message received callback
    using MessageCallback = std::function<void(const juce::String &topic, const juce::String &message)>;

    //==============================================================================
    MqttClient();
    ~MqttClient() override;

    // Connection management
    void connect(const juce::String &brokerUrl,
                 const juce::String &clientId = "",
                 const juce::String &username = "",
                 const juce::String &password = "");
    void disconnect();

    // Publishing
    void publish(const juce::String &topic, const juce::String &message, int qos = 0, bool retain = false);

    // Subscription
    void subscribe(const juce::String &topic);
    void unsubscribe(const juce::String &topic);

    // Callbacks
    void setConnectionCallback(ConnectionCallback callback);
    void setMessageCallback(MessageCallback callback);

    // Status
    bool getConnectionStatus() const { return isConnected.load(); }
    juce::StringArray getSubscribedTopics() const;

private:
    // Thread implementation
    void run() override;

    // MQTT client (C library)
    MQTTAsync client;

    // Connection parameters
    juce::String brokerUrl;
    juce::String clientId;
    juce::String username;
    juce::String password;

    // State
    std::atomic<bool> isConnected{false};
    std::atomic<bool> shouldConnect{false};

    // Callbacks
    ConnectionCallback connectionCallback;
    MessageCallback messageCallback;

    // Subscriptions
    juce::StringArray subscribedTopics;
    mutable juce::CriticalSection subscriptionsMutex;

    // Paho C callback functions (static)
    static void onConnectionLost(void *context, char *cause);
    static int onMessageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message);
    static void onDeliveryComplete(void *context, MQTTAsync_token token);

    // Connection callback handlers
    static void onConnectSuccess(void *context, MQTTAsync_successData *response);
    static void onConnectFailure(void *context, MQTTAsync_failureData *response);

    // Internal helper methods
    void attemptConnection();
    void handleConnectionResult(bool success, const juce::String &error = "");
    void handleMessage(const juce::String &topic, const juce::String &message);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MqttClient)
};
