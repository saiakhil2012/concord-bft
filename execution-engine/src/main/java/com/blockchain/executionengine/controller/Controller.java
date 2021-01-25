package com.blockchain.executionengine.controller;

import com.blockchain.executionengine.command.Command;
import com.blockchain.executionengine.store.KeyValue;
import com.blockchain.executionengine.store.KeyObject;
import com.fasterxml.jackson.databind.util.JSONPObject;
import com.google.gson.Gson;
import lombok.extern.slf4j.Slf4j;
import org.jasypt.encryption.pbe.StandardPBEStringEncryptor;
import org.springframework.boot.configurationprocessor.json.JSONObject;
import org.springframework.http.HttpEntity;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;

import org.springframework.web.client.RestTemplate;

import javax.crypto.Cipher;
import java.util.HashMap;

@Slf4j
@RestController
class Controller {
    Gson g = new Gson();
    RestTemplate restTemplate = new RestTemplate();
    String dbUrl = "http://172.17.0.1:9090/db";

    String seed = "TestSeedCanBeGeneratedAndStoredPerUser";

    HashMap<String, String> keyToEncryptedKey = new HashMap<>();


    // Aggregate root

    @GetMapping("/test")
    String testAPI() {
        return "Tested";
    }

    @GetMapping("/ee/{key}")
    String getValue(@PathVariable String key) {
        log.debug("Get and now calling db");
        return restTemplate.getForObject(dbUrl + "/" + key, String.class);
    }

    @PostMapping("/ee/execute")
    String newKeyValue(@RequestBody String request) {
        log.debug("Post and now calling db");
        log.debug("Request is " + request);
        try {
            Command command = new Command();
            JSONObject reqObject = new JSONObject(request);
            if (reqObject.has("command")) {
                if (reqObject.has("key")) {
                    if (reqObject.has("value")) {
                        command = new Command(reqObject.getString("command"), reqObject.getString("key"), reqObject.getString("value"));
                    } else {
                        command = new Command(reqObject.getString("command"), reqObject.getString("key"), "");
                    }
                }
            }
            if (command.getCommandType().equals("get")) {
                log.debug("Key is " + command.getKey());
                String response = restTemplate.getForObject(dbUrl + "/" + command.getKey(), String.class);
                log.debug("Response is " + response);
                return response;
            } else if (command.getCommandType().equals("add")) {
                KeyValue keyValue = new KeyValue(command.getKey(), command.getValue());
                String requestJson = g.toJson(keyValue);
                HttpHeaders headers = new HttpHeaders();
                headers.setContentType(MediaType.APPLICATION_JSON);

                HttpEntity<String> entity = new HttpEntity<String>(requestJson,headers);
                return restTemplate.postForObject(dbUrl + "/kv", entity, String.class);
            } else if (command.getCommandType().equals("remove")) {
                String requestJson = g.toJson(command);
                HttpHeaders headers = new HttpHeaders();
                headers.setContentType(MediaType.APPLICATION_JSON);

                HttpEntity<String> entity = new HttpEntity<String>(requestJson,headers);
                restTemplate.delete(dbUrl + "/kv" + command.getKey());
                return command.getKey();
            }
        } catch (Exception exception) {
            log.error("Exception when parsing json ", exception);
            return "Invalid JSON Body";
        }
        return "Not a valid command type";
    }

    @PostMapping("/ee/secured/execute")
    String newSecuredKeyValue(@RequestBody String request) {
        log.debug("Secured Post and now calling db");
        log.debug("Request is " + request);
        try {
            StandardPBEStringEncryptor encryptor = new StandardPBEStringEncryptor();
            encryptor.setPassword(seed);

            Command command = new Command();
            JSONObject reqObject = new JSONObject(request);
            if (reqObject.has("command")) {
                if (reqObject.has("key")) {
                    if (reqObject.has("value")) {
                        command = new Command(reqObject.getString("command"), encryptor.encrypt(reqObject.getString("key")),
                                encryptor.encrypt(reqObject.getString("value")));
                    } else {
                        command = new Command(reqObject.getString("command"), (reqObject.getString("key")), "");
                    }
                }
            }
            if (command.getCommandType().equals("get")) {
                log.debug("Key is " + command.getKey());
                String encryptedKey = keyToEncryptedKey.get(command.getKey());
                log.debug("Key was encrypted as " + encryptedKey);
                KeyObject key = new KeyObject(encryptedKey);
                String requestJson = g.toJson(key);
                log.debug("Request JSON to SKVBC is " + requestJson);
                HttpHeaders headers = new HttpHeaders();
                headers.setContentType(MediaType.APPLICATION_JSON);

                HttpEntity<String> entity = new HttpEntity<String>(requestJson,headers);
                String response = encryptor.decrypt(restTemplate.postForObject(dbUrl + "/key", entity, String.class));
                log.debug("Response is " + response);
                return response;
            } else if (command.getCommandType().equals("add")) {
                log.debug("Key after encrypt is " + command.getKey());
                // Storing Key --> Encrypted Key in the Map
                keyToEncryptedKey.put(encryptor.decrypt(command.getKey()), command.getKey());
                KeyValue keyValue = new KeyValue(command.getKey(), command.getValue());
                String requestJson = g.toJson(keyValue);
                log.debug("Request JSON to SKVBC is " + requestJson);
                HttpHeaders headers = new HttpHeaders();
                headers.setContentType(MediaType.APPLICATION_JSON);

                HttpEntity<String> entity = new HttpEntity<String>(requestJson,headers);
                return restTemplate.postForObject(dbUrl + "/kv", entity, String.class);
            } else if (command.getCommandType().equals("remove")) {
                String requestJson = g.toJson(command);
                HttpHeaders headers = new HttpHeaders();
                headers.setContentType(MediaType.APPLICATION_JSON);

                HttpEntity<String> entity = new HttpEntity<String>(requestJson,headers);
                restTemplate.delete(dbUrl + "/kv" + command.getKey());
                return command.getKey();
            }
        } catch (Exception exception) {
            log.error("Exception when parsing json ", exception);
            exception.printStackTrace();
            return "Invalid JSON Body";
        }
        return "Not a valid command type";
    }
}