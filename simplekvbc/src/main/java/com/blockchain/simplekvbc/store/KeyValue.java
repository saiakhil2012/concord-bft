package com.blockchain.simplekvbc.store;

import lombok.Builder;
import lombok.Getter;
import lombok.Setter;

@Getter
@Setter
@Builder
public class KeyValue {
    private String commandType;
    private String key;
    private String value;
}