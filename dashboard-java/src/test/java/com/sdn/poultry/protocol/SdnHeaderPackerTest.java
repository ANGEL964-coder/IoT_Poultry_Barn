package com.sdn.poultry.protocol;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;

public class SdnHeaderPackerTest {

    @Test
    @DisplayName("Should pack and unpack 32-bit Little-Endian RFC 7426 control word")
    public void testPackAndUnpack() {
        int cmdId = 1;       // RECONFIG_RADIO
        int sfTarget = 2;    // SF9
        int txPower = 14;    // 14 dBm
        int sampleRate = 1;  // 60s
        int payloadMode = 1; // CRITICAL
        int sleepEnable = 0; // false
        int paramValue = 0;

        byte[] packed = SdnHeaderPacker.pack(cmdId, sfTarget, txPower, sampleRate, payloadMode, sleepEnable, paramValue);
        assertThat(packed).hasSize(4);

        Map<String, Object> unpacked = SdnHeaderPacker.unpack(packed);
        assertThat(unpacked.get("cmdId")).isEqualTo(cmdId);
        assertThat(unpacked.get("cmdName")).isEqualTo("RECONFIG_RADIO");
        assertThat(unpacked.get("sfTarget")).isEqualTo(sfTarget);
        assertThat(unpacked.get("sfName")).isEqualTo("SF9 (DR1)");
        assertThat(unpacked.get("txPower")).isEqualTo(txPower);
        assertThat(unpacked.get("sampleRateIndex")).isEqualTo(sampleRate);
        assertThat(unpacked.get("sampleRateSeconds")).isEqualTo(60);
        assertThat(unpacked.get("payloadMode")).isEqualTo(payloadMode);
        assertThat(unpacked.get("sleepEnable")).isEqualTo(0);
        assertThat(unpacked.get("paramValue")).isEqualTo(paramValue);
    }

    @Test
    @DisplayName("Should correctly generate Base64 and Hex representations")
    public void testBase64AndHexRepresentations() {
        byte[] packed = SdnHeaderPacker.pack(1, 0, 14, 0, 0, 0, 0);
        String hex = SdnHeaderPacker.toHex(packed);
        String b64 = SdnHeaderPacker.toBase64(packed);

        assertThat(hex).isNotEmpty();
        assertThat(b64).isNotEmpty();
        assertThat(b64).hasSize(4 * ((packed.length + 2) / 3)); // Standard Base64 length with padding
    }
}
