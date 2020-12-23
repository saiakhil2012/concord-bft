package com.blockchain.executionengine.store;

import lombok.*;

@Getter
@Setter
@Builder
@AllArgsConstructor
public class KeyValue {
    private String key;
    private String value;
}
