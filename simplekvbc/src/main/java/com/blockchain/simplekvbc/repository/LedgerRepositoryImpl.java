package com.blockchain.simplekvbc.repository;

import lombok.extern.slf4j.Slf4j;
import org.rocksdb.Options;
import org.rocksdb.RocksDB;
import org.rocksdb.RocksDBException;
import org.springframework.stereotype.Repository;

import javax.annotation.PostConstruct;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;

@Slf4j
@Repository
public class LedgerRepositoryImpl implements LedgerRepository<String, String> {

    private final static String NAME = "first-db";
    File dbDir;
    RocksDB db;

    @PostConstruct
    void initialize() {
        RocksDB.loadLibrary();
        final Options options = new Options();
        options.setCreateIfMissing(true);
        dbDir = new File("/tmp/rocks-db", NAME);
        try {
            Files.createDirectories(dbDir.getParentFile().toPath());
            Files.createDirectories(dbDir.getAbsoluteFile().toPath());
            db = RocksDB.open(options, dbDir.getAbsolutePath());
        } catch (IOException | RocksDBException ex) {
            log.error("Error initializng RocksDB, check configurations and permissions, exception: {}, message: {}, stackTrace: {}",
                    ex.getCause(), ex.getMessage(), ex.getStackTrace());
        }
        log.info("RocksDB initialized and ready to use");
    }

    @Override
    public synchronized void save(String key, String value) {
        log.debug("save");
        try {
            db.put(key.getBytes(), value.getBytes());
        } catch (RocksDBException e) {
            log.error("Error saving entry in RocksDB, cause: {}, message: {}", e.getCause(), e.getMessage());
        }
    }

    @Override
    public String find(String key) {
        log.debug("find");
        String result = null;
        try {
            byte[] bytes = db.get(key.getBytes());
            if (bytes == null) return null;
            result = new String(bytes);
        } catch (RocksDBException e) {
            log.error("Error retrieving the entry in RocksDB from key: {}, cause: {}, message: {}", key, e.getCause(), e.getMessage());
        }
        return result;
    }

    @Override
    public void delete(String key) {
        log.debug("delete");
        try {
            db.delete(key.getBytes());
        } catch (RocksDBException e) {
            log.error("Error deleting entry in RocksDB, cause: {}, message: {}", e.getCause(), e.getMessage());
        }
    }
}
