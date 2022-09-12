/* MIT License

Copyright (c) 2020 Jason C. Fain

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#pragma once

#include <AsyncJson.h>
#include <list>

AsyncWebSocket ws("/ws");
#include <mutex>


class WebSocketHandler {
    public: 
        void setup(AsyncWebServer* server) {
            LogHandler::info(_TAG, "Setting up webSocket");
            ws.onEvent([&](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
                onWsEvent(server, client, type, arg, data, len);
            });
            server->addHandler(&ws);
            tCodeInQueue = xQueueCreate(5, sizeof(char[255]));
            if(tCodeInQueue == NULL) {
                LogHandler::error(_TAG, "Error creating the tcode queue");
            }
            debugInQueue = xQueueCreate(50, sizeof(char[255]));
            if(debugInQueue == NULL) {
                LogHandler::error(_TAG, "Error creating the debug queue");
            }
            isInitialized = true;
            xTaskCreate(this->emptyQueue, "emptyDebugQueue", 4096, this, tskIDLE_PRIORITY, emptyQueueHandle);
        }
        
        void CommandCallback(const String& in){ //This overwrites the callback for message return
            if(isInitialized && ws.count() > 0)
                sendCommand(in.c_str());
        }

        void sendDebug(const String message) {
            if (isInitialized && serial_mtx.try_lock()) {
                std::lock_guard<std::mutex> lck(serial_mtx, std::adopt_lock);
                    // Serial.print("insert to q: ");
                    // Serial.println(message);
                    xQueueSend(debugInQueue, message.c_str(), 0);
            }
        }

        void sendCommand(const char* command, const char* message = 0, AsyncWebSocketClient* client = 0)
        {
            if(isInitialized && command_mtx.try_lock()) {
                std::lock_guard<std::mutex> lck(command_mtx, std::adopt_lock);
                m_lastSend = millis();
                // if(message)
                //     Serial.printf("Sending WS command: %s, Message: %s", command, message);
                // else
                //     Serial.printf("Sending WS command: %s", command);
                char commandJson[255];
                if(!message)
                    sprintf(commandJson, "{ \"command\": \"%s\" }", command);
                else if(strpbrk(message, "{") != nullptr)
                    sprintf(commandJson, "{ \"command\": \"%s\" , \"message\": %s }", command, message);
                else
                    sprintf(commandJson, "{ \"command\": \"%s\" , \"message\": \"%s\" }", command, message);
                if(client)
                    client->text(commandJson);
                else
                    ws.textAll(commandJson);
            }
        }

        void getTCode(char* webSocketData) 
        {
            if(tCodeInQueue == NULL)
            {
                LogHandler::error(_TAG, "TCode queue was null");
                return;
            } 
			if(xQueueReceive(tCodeInQueue, webSocketData, 0)) 
            {
                //tcode->toCharArray(webSocketData, tcode->length() + 1);
                // Serial.print("Top tcode: ");
                // Serial.println(webSocketData);
            }
            else 
            {
      	        webSocketData[0] = {0};
            }
            ws.cleanupClients();
        }

        void closeAll() 
        {
            for (AsyncWebSocketClient *pClient : m_clients)
                pClient->close();
        }

    private:
        bool isInitialized = false;
        std::mutex serial_mtx;
        std::mutex command_mtx;
        const char* _TAG = "WebSocket";
// unsigned long lastCall;
        std::list<AsyncWebSocketClient *> m_clients;
        QueueHandle_t tCodeInQueue;
        static QueueHandle_t debugInQueue;
        static int m_lastSend;
        static TaskHandle_t* emptyQueueHandle;
        static bool emptyQueueRunning;

        static void emptyQueue(void *webSocketHandler) {
            while (true) {
                if(ws.count() > 0 && millis() - m_lastSend > 30 && uxQueueMessagesWaiting(debugInQueue)) {
				    char lastMessage[255];
                    if(xQueueReceive(debugInQueue, lastMessage, 0)) {
                        // Serial.printf("read from q: %s\n", lastMessage);
                        ((WebSocketHandler*)webSocketHandler)->sendCommand("debug", lastMessage);
                    }
                }
        	    vTaskDelay(30/portTICK_PERIOD_MS);
            }
            vTaskDelete(NULL);
        }

        void processWebSocketTextMessage(char* msg) 
        {
            if(strpbrk(msg, "{") == nullptr)  
            {
                if(tCodeInQueue == NULL)
                {
                    LogHandler::error(_TAG, "TCode queue was null");
                } 
                else 
                {
                    
                    LogHandler::verbose(_TAG, "Websocket tcode in: %s", msg);
                    xQueueSend(tCodeInQueue, msg, 0);
	// Serial.print("Time between ws calls: ");
	// Serial.println(millis() - lastCall);
	// //Serial.println(msg);
	// lastCall = millis();
                    //executeTCode(msg);
                }
                // if (strcmp(msg, SettingsHandler::HandShakeChannel) == 0) 
                // {
                //     sendCommand(SettingsHandler::TCodeVersionName);
                // }
            }
            else
            {
                DynamicJsonDocument doc(255);
                DeserializationError error = deserializeJson(doc, msg);
                if (error) 
                {
                    LogHandler::error(_TAG, "Failed to read websocket json");
                    return;
                }
                JsonObject jsonObj = doc.as<JsonObject>();

                if(!jsonObj["command"].isNull()) 
                {
                    // String* message = jsonObj["message"];
                    // Serial.print("Recieved websocket tcode message: ");
                    // Serial.println(message->c_str());
                    // if(tCodeInQueue == NULL)return;
			        // xQueueSend(tCodeInQueue, &message, 0);
                } 
                else 
                {
                    LogHandler::verbose(_TAG, "Websocket tcode in JSON: %s", msg);
                    char tcode[255];
                    SettingsHandler::processTCodeJson(tcode, msg);
                    // Serial.print("tcode JSON converted:");
                    // Serial.println(tcode);
			        xQueueSend(tCodeInQueue, tcode, 0);
                }
            }
        }

        void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
        {
            if(type == WS_EVT_CONNECT)
            {
                LogHandler::debug(_TAG, "ws[%s][%u] connect\n", server->url(), client->id());
                //client->printf("Hello Client %u :)", client->id());
                // client->ping();
                // client->client()->setNoDelay(true);
                m_clients.push_back(client);
            } 
            else if(type == WS_EVT_DISCONNECT)
            {
                LogHandler::debug(_TAG, "ws[%s][%u] disconnect\n", server->url(), client->id());
                m_clients.remove(client);
            } 
            else if(type == WS_EVT_ERROR)
            {
                LogHandler::debug(_TAG, "ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
            } 
            else if(type == WS_EVT_PONG)
            {
                LogHandler::debug(_TAG, "ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
            } 
            else if(type == WS_EVT_DATA)
            {
                AwsFrameInfo * info = (AwsFrameInfo*)arg;
                //String msg = "";
                if(info->final && info->index == 0 && info->len == len)
                {
                    //the whole message is in a single frame and we got all of it's data
                    //Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

                    // if(info->opcode == WS_TEXT)
                    // {
                    //     for(size_t i=0; i < info->len; i++) 
                    //     {
                    //         msg += (char) data[i];
                    //     }
                    // } 
                    // else 
                    // {
                    //     char buff[3];
                    //     for(size_t i=0; i < info->len; i++) 
                    //     {
                    //         sprintf(buff, "%02x ", (uint8_t) data[i]);
                    //         msg += buff ;
                    //     }
                    // }
                    // Serial.printf("%s\n",msg.c_str());

                    if(info->opcode == WS_TEXT) 
                    {
                        data[len] = 0;
                        processWebSocketTextMessage((char*) data);
                    }
                    else
                        client->binary("I got your binary message");
                } 
                else 
                {
                //message is comprised of multiple frames or the frame is split into multiple packets
                    // if(info->index == 0)
                    // {
                    //     if(info->num == 0)
                    //         Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
                    //     Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
                    // }

                    // Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

                    // if(info->opcode == WS_TEXT)
                    // {
                    //     for(size_t i=0; i < len; i++) 
                    //     {
                    //         msg += (char) data[i];
                    //     }
                    // } 
                    // else 
                    // {
                    //     char buff[3];
                    //     for(size_t i=0; i < len; i++) 
                    //     {
                    //         sprintf(buff, "%02x ", (uint8_t) data[i]);
                    //         msg += buff ;
                    //     }
                    // }
                    // Serial.printf("%s\n",msg.c_str());

                    if((info->index + len) == info->len)
                    {
                        //Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
                        if(info->final)
                        {
                            //Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
                            if(info->message_opcode == WS_TEXT) 
                            {
                                data[len] = 0;
                                processWebSocketTextMessage((char*)data);
                            }
                            else
                                client->binary("I got your binary message");
                        }
                    }
                }
            }
        }
};

bool WebSocketHandler::emptyQueueRunning = false;
QueueHandle_t WebSocketHandler::debugInQueue;
int WebSocketHandler::m_lastSend = 0;
TaskHandle_t* WebSocketHandler::emptyQueueHandle = NULL;