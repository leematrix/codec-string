//
// Created by Matrix on 2023/12/2.
//

#ifndef EM_PORT_API
#	if defined(__EMSCRIPTEN__)
#		include <emscripten.h>
#		if defined(__cplusplus)
#			define EM_PORT_API(rettype) extern "C" rettype EMSCRIPTEN_KEEPALIVE
#		else
#			define EM_PORT_API(rettype) rettype EMSCRIPTEN_KEEPALIVE
#		endif
#	else
#		if defined(__cplusplus)
#			define EM_PORT_API(rettype) extern "C" rettype
#		else
#			define EM_PORT_API(rettype) rettype
#		endif
#	endif
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t frame_cache[1000000]={0};
static size_t frame_size = 82034;

void read_keyframe() {
    static FILE *fp_inH264 = NULL;
    static const char* file_name = "/Users/lzx/Movies/AV/h265/ipc_video.265";

    fp_inH264 = fopen(file_name, "r");
    if (fp_inH264 == NULL) {
        printf("fopen error\n.");
        goto end;
    }

    int n = fread(frame_cache, 1, frame_size, fp_inH264);
    if (n != 82034) {
        printf("fread error, n:%d\n.", n);
        goto end;
    }
end:
    fclose(fp_inH264);
}

static char codec_string[100] = {0};

typedef struct hevc_profile_tier_level{
    uint8_t  general_profile_space;	// 2bit,[0,3]
    uint8_t  general_tier_flag;		// 1bit,[0,1]
    uint8_t  general_profile_idc;	// 5bit,[0,31]
    uint32_t general_profile_compatibility_flags;
    uint8_t general_constraint_indicator_flags[6];
    uint8_t  general_level_idc;
} hevc_profile_tier_level_t;

char *generate_hevc_codec_string(hevc_profile_tier_level_t *hevc) {
    char tmp[20] = {0};

    memset(codec_string, 0, sizeof(codec_string));
    // 示例：hev1.1.6.L123.B0
    size_t index = 0;
    sprintf(codec_string, "hev1.");
    index += 5;

    int i;
    if (hevc) {
        switch (hevc->general_profile_space) {
            case 0:
                break;
            case 1:
                sprintf(codec_string + index, "A");
                index++;
                break;
            case 2:
                sprintf(codec_string + index, "B");
                index++;
                break;
            case 3:
                sprintf(codec_string + index, "C");
                index++;
                break;
        }
        sprintf(tmp, "%d", hevc->general_profile_idc);
        sprintf(codec_string + index, "%s.", tmp);
        index += strlen(tmp) + 1;

        uint32_t val = hevc->general_profile_compatibility_flags;
        uint32_t reversed = 0;
        for (i=0; i<32; i++) {
            reversed |= val & 1;
            if (i==31) break;
            reversed <<= 1;
            val >>= 1;
        }

        memset(tmp, 0, sizeof(tmp));
        sprintf(tmp, "%x", reversed);
        sprintf(codec_string + index, "%s.", tmp);
        index += strlen(tmp) + 1;

        if (hevc->general_tier_flag == 0) {
            sprintf(codec_string + index, "L");
        } else {
            sprintf(codec_string + index, "H");
        }
        index++;

        memset(tmp, 0, sizeof(tmp));
        sprintf(tmp, "%d", hevc->general_level_idc);
        sprintf(codec_string + index, "%s", tmp);
        index += strlen(tmp);

        int hasByte = 0;
        for (i = 5; i >= 0; i--) {
            if (hevc->general_constraint_indicator_flags[i] || hasByte) {
                memset(tmp, 0, sizeof(tmp));
                sprintf(tmp, "%x", hevc->general_constraint_indicator_flags[i]);
                sprintf(codec_string + index, ".%s", tmp);
                index += strlen(tmp)+1;
                hasByte = 1;
            }
        }
    }
    return codec_string;
}

size_t hevc_find_vps(const uint8_t *frame, size_t size){
    size_t index = 0;
    for (; index < size; index++) {
        if (frame[index] == 0 && frame[index+1] == 0 && (frame[index+2] == 1)) {
            index += 3;
        } else if (frame[index] == 0 && frame[index+1] == 0 && frame[index+2] == 0 && frame[index+3] == 1) {
            index += 4;
        } else {
            continue;
        }

        if ((frame[index] & 0x7E) >> 1 == 32 && frame[index+1] == 1) {
            index += 2;
            break;
        }
    }
    return index;
}

int decode_hevc_profile_tier_level(const uint8_t* nalu, size_t bytes, hevc_profile_tier_level_t *hevc) {
    if (bytes < 12) {
        return -1;
    }
    hevc->general_profile_space = (nalu[0] >> 6) & 0x03;
    hevc->general_tier_flag = (nalu[0] >> 5) & 0x01;
    hevc->general_profile_idc = nalu[0] & 0x1f;

    hevc->general_profile_compatibility_flags = 0;
    hevc->general_profile_compatibility_flags |= nalu[1] << 24;
    hevc->general_profile_compatibility_flags |= nalu[2] << 16;
    hevc->general_profile_compatibility_flags |= nalu[3] << 8;
    hevc->general_profile_compatibility_flags |= nalu[4];

    hevc->general_constraint_indicator_flags[0] = nalu[5];
    hevc->general_constraint_indicator_flags[1] = nalu[6];
    hevc->general_constraint_indicator_flags[2] = nalu[7];
    hevc->general_constraint_indicator_flags[3] = nalu[8];
    hevc->general_constraint_indicator_flags[4] = nalu[9];
    hevc->general_constraint_indicator_flags[5] = nalu[10];

    hevc->general_level_idc = nalu[11];
    return 0;
}

int remove_contention_code(uint8_t *dst, size_t dst_size, const uint8_t *src, size_t src_size) {
    if (dst_size < 16) {
        return -1;
    }

    dst[0] = src[0];
    dst[1] = src[1];
    size_t dst_index = 2;

    size_t i = 2;
    for (; i < src_size && dst_index < 16; i++) {
        if (src[i-2] == 0 && src[i-1] == 0 && src[i] == 3) {
            continue;
        }
        dst[dst_index++] = src[i];
    }
    if (dst_index == 16) {
        return 0;
    }
    return -1;
}

EM_PORT_API(char *) parse_hevc_codec_string(const uint8_t *frame, size_t size) {
    if (size < 20) {
        return "";
    }

    size_t vps_index = hevc_find_vps(frame, size);
    if (vps_index >= size) {
        return "";
    }

    uint8_t buffer[16] = {0};
    if (-1 == remove_contention_code(buffer, sizeof(buffer), frame + vps_index, size - vps_index)) {
        return "";
    }

    hevc_profile_tier_level_t hevc;
    decode_hevc_profile_tier_level(buffer + 4, sizeof(buffer) - 4, &hevc);

    return generate_hevc_codec_string(&hevc);
}

size_t avc_find_sps(const uint8_t *frame, size_t size){
    size_t index = 0;
    for (; index < size; index++) {
        if (frame[index] == 0 && frame[index+1] == 0 && (frame[index+2] == 1)) {
            index += 3;
        } else if (frame[index] == 0 && frame[index+1] == 0 && frame[index+2] == 0 && frame[index+3] == 1) {
            index += 4;
        } else {
            continue;
        }

        if ((frame[index] & 0x0F) == 7) {
            index++;
            break;
        }
    }
    return index;
}

EM_PORT_API(char *) parse_avc_codec_string(const uint8_t *frame, size_t size) {
    if (size < 7) {
        return "";
    }

    size_t sps_index = avc_find_sps(frame, size);
    if (sps_index >= size) {
        return "";
    }

    if (sps_index >= size) {
        return "";
    }

    memset(codec_string, 0, sizeof(codec_string));
    sprintf(codec_string, "avc1.%02x%02x%02x", frame[sps_index], frame[sps_index+1], frame[sps_index+2]);
    return codec_string;
}

int main (int argc, char**argv) {
    read_keyframe();
    char *result = parse_hevc_codec_string(frame_cache, frame_size);
    printf("codec_string:%s\n", result);
    return 0;
}
