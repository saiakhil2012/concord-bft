package com.blockchain.simplekvbc.repository;

public interface LedgerRepository<K, V> {
    void save(K key, V value);
    V find(K key);
    void delete(K key);
}
