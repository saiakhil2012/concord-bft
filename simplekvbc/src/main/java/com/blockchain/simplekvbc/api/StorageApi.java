package com.blockchain.simplekvbc.api;

import com.blockchain.simplekvbc.store.KeyValue;
import com.blockchain.simplekvbc.repository.LedgerRepository;
import lombok.extern.slf4j.Slf4j;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@Slf4j
@RestController
public class StorageApi {
    private final LedgerRepository<String, String> rocksDB;

    public StorageApi(LedgerRepository<String, String> rocksDB) {
        this.rocksDB = rocksDB;
    }

    @PostMapping("/db/kv")
    public ResponseEntity<String> save(@RequestBody KeyValue keyValue) {
        log.info("RocksApi.save " + keyValue.getKey() + " : " + keyValue.getValue());
        rocksDB.save(keyValue.getKey(), keyValue.getValue());
        return ResponseEntity.ok(keyValue.getValue());
    }

    @GetMapping("/db/{key}")
    public ResponseEntity<String> find(@PathVariable("key") String key) {
        log.info("RocksApi.find " + key);
        String result = rocksDB.find(key);
        log.info("Response is " + result);
        if(result == null) return ResponseEntity.noContent().build();
        return ResponseEntity.ok(result);
    }

    @DeleteMapping("/db/{key}")
    public ResponseEntity<String> delete(@PathVariable("key") String key) {
        log.info("RocksApi.delete " + key);
        rocksDB.delete(key);
        return ResponseEntity.ok(key);
    }
}
