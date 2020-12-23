package com.blockchain.executionengine.command;

import lombok.*;

@Getter
@Setter
@Builder
@AllArgsConstructor
@NoArgsConstructor
public class Command {
    private String commandType = "";
    private String key = "";
    private String value = "";
}