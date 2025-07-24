#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <alsa/asoundlib.h>

// WAV file header structure
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t chunk_size;    // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // Format chunk size
    uint16_t audio_format;  // Audio format (1 = PCM)
    uint16_t num_channels;  // Number of channels
    uint32_t sample_rate;   // Sample rate
    uint32_t byte_rate;     // Bytes per second
    uint16_t block_align;   // Bytes per sample frame
    uint16_t bits_per_sample; // Bits per sample
    char data[4];           // "data"
    uint32_t data_size;     // Data chunk size
} WAVHeader;

typedef struct {
    int16_t *samples;
    uint32_t sample_count;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
} AudioData;

int read_wav_file(const char *filename, AudioData *audio) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Could not open file %s\n", filename);
        return -1;
    }
    
    // Read RIFF header
    char riff_header[12];
    if (fread(riff_header, 1, 12, file) != 12) {
        printf("Error: Could not read RIFF header\n");
        fclose(file);
        return -1;
    }
    
    // Verify RIFF and WAVE
    if (strncmp(riff_header, "RIFF", 4) != 0 || strncmp(riff_header + 8, "WAVE", 4) != 0) {
        printf("Error: Not a valid WAV file\n");
        fclose(file);
        return -1;
    }
    
    // Initialize audio structure
    memset(audio, 0, sizeof(AudioData));
    
    // Read chunks until we find fmt and data
    int found_fmt = 0, found_data = 0;
    long data_pos = 0;
    uint32_t data_size = 0;
    
    while (!feof(file) && (!found_fmt || !found_data)) {
        char chunk_id[4];
        uint32_t chunk_size;
        
        if (fread(chunk_id, 1, 4, file) != 4) break;
        if (fread(&chunk_size, 4, 1, file) != 1) break;
        
        if (strncmp(chunk_id, "fmt ", 4) == 0) {
            // Read format chunk
            uint16_t audio_format;
            uint16_t num_channels;
            uint32_t sample_rate;
            uint32_t byte_rate;
            uint16_t block_align;
            uint16_t bits_per_sample;
            
            fread(&audio_format, 2, 1, file);
            fread(&num_channels, 2, 1, file);
            fread(&sample_rate, 4, 1, file);
            fread(&byte_rate, 4, 1, file);
            fread(&block_align, 2, 1, file);
            fread(&bits_per_sample, 2, 1, file);
            
            // Skip any extra format data
            if (chunk_size > 16) {
                fseek(file, chunk_size - 16, SEEK_CUR);
            }
            
            if (audio_format != 1) {
                printf("Error: Only PCM format supported (found format %d)\n", audio_format);
                fclose(file);
                return -1;
            }
            
            if (bits_per_sample != 16) {
                printf("Error: Only 16-bit samples supported (found %d-bit)\n", bits_per_sample);
                fclose(file);
                return -1;
            }
            
            audio->sample_rate = sample_rate;
            audio->channels = num_channels;
            audio->bits_per_sample = bits_per_sample;
            found_fmt = 1;
            
            printf("WAV File Info:\n");
            printf("  Sample Rate: %d Hz\n", sample_rate);
            printf("  Channels: %d\n", num_channels);
            printf("  Bits per Sample: %d\n", bits_per_sample);
            
        } else if (strncmp(chunk_id, "data", 4) == 0) {
            // Found data chunk
            data_pos = ftell(file);
            data_size = chunk_size;
            found_data = 1;
            printf("  Data Size: %d bytes\n", data_size);
            
            // Skip data for now
            fseek(file, chunk_size, SEEK_CUR);
            
        } else {
            // Skip unknown chunk
            fseek(file, chunk_size, SEEK_CUR);
        }
    }
    
    if (!found_fmt) {
        printf("Error: No format chunk found\n");
        fclose(file);
        return -1;
    }
    
    if (!found_data) {
        printf("Error: No data chunk found\n");
        fclose(file);
        return -1;
    }
    
    // Allocate memory for samples
    audio->sample_count = data_size / sizeof(int16_t);
    audio->samples = malloc(data_size);
    
    if (!audio->samples) {
        printf("Error: Could not allocate memory for audio data\n");
        fclose(file);
        return -1;
    }
    
    // Seek to data and read it
    fseek(file, data_pos, SEEK_SET);
    size_t samples_read = fread(audio->samples, sizeof(int16_t), audio->sample_count, file);
    if (samples_read != audio->sample_count) {
        printf("Warning: Only read %zu of %d samples\n", samples_read, audio->sample_count);
        audio->sample_count = samples_read;
    }
    
    fclose(file);
    printf("Successfully loaded %d samples\n", audio->sample_count);
    return 0;
}

int read_raw_file(const char *filename, AudioData *audio, uint32_t sample_rate, uint16_t channels) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Could not open file %s\n", filename);
        return -1;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    audio->sample_count = file_size / sizeof(int16_t);
    audio->samples = malloc(file_size);
    audio->sample_rate = sample_rate;
    audio->channels = channels;
    audio->bits_per_sample = 16;
    
    if (!audio->samples) {
        printf("Error: Could not allocate memory\n");
        fclose(file);
        return -1;
    }
    
    fread(audio->samples, sizeof(int16_t), audio->sample_count, file);
    fclose(file);
    
    printf("Successfully loaded raw file: %d samples at %d Hz\n", 
           audio->sample_count, sample_rate);
    return 0;
}

int play_audio_alsa(AudioData *audio) {
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    int dir;
    int pcm;
    
    // Open PCM device for playback
    pcm = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (pcm < 0) {
        printf("ERROR: Can't open PCM device. %s\n", snd_strerror(pcm));
        return -1;
    }
    
    // Allocate parameters object
    snd_pcm_hw_params_alloca(&params);
    
    // Get current hardware parameters
    pcm = snd_pcm_hw_params_any(pcm_handle, params);
    if (pcm < 0) {
        printf("ERROR: Can't get hardware parameters. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        return -1;
    }
    
    // Set access type
    pcm = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (pcm < 0) {
        printf("ERROR: Can't set access type. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        return -1;
    }
    
    // Set sample format
    pcm = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    if (pcm < 0) {
        printf("ERROR: Can't set format. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        return -1;
    }
    
    // Try to set the original channel count, but be flexible
    unsigned int device_channels = audio->channels;
    pcm = snd_pcm_hw_params_set_channels_near(pcm_handle, params, &device_channels);
    if (pcm < 0) {
        printf("ERROR: Can't set any channel configuration. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        return -1;
    }
    
    if (device_channels != audio->channels) {
        printf("Note: Audio has %d channels, device supports %d channels\n", 
               audio->channels, device_channels);
        printf("Will convert audio automatically.\n");
    }
    
    // Set sample rate
    unsigned int rate = audio->sample_rate;
    pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, &dir);
    if (pcm < 0) {
        printf("ERROR: Can't set rate. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        return -1;
    }
    
    if (rate != audio->sample_rate) {
        printf("Warning: Requested rate %d Hz, got %d Hz\n", audio->sample_rate, rate);
    }
    
    // Set buffer size (in frames)
    snd_pcm_uframes_t buffer_size = (audio->sample_count / audio->channels) / 4;
    if (buffer_size < 1024) buffer_size = 1024;
    if (buffer_size > 32768) buffer_size = 32768;
    
    pcm = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, params, &buffer_size);
    if (pcm < 0) {
        printf("ERROR: Can't set buffer size. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        return -1;
    }
    
    // Set period size
    frames = buffer_size / 8;
    if (frames < 64) frames = 64;
    if (frames > 8192) frames = 8192;
    
    pcm = snd_pcm_hw_params_set_period_size_near(pcm_handle, params, &frames, &dir);
    if (pcm < 0) {
        printf("ERROR: Can't set period size. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        return -1;
    }
    
    // Apply parameters
    pcm = snd_pcm_hw_params(pcm_handle, params);
    if (pcm < 0) {
        printf("ERROR: Can't set hardware parameters. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle);
        return -1;
    }
    
    // Get the actual values
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
    
    printf("ALSA Configuration:\n");
    printf("  Rate: %d Hz\n", rate);
    printf("  Device Channels: %d\n", device_channels);
    printf("  Period size: %lu frames\n", frames);
    printf("  Buffer size: %lu frames\n", buffer_size);
    printf("Playing audio...\n");
    
    // Create conversion buffer if needed
    int16_t *play_buffer = audio->samples;
    int play_frames = audio->sample_count / audio->channels;
    int need_conversion = (device_channels != audio->channels);
    
    if (need_conversion) {
        play_buffer = malloc(play_frames * device_channels * sizeof(int16_t));
        if (!play_buffer) {
            printf("ERROR: Can't allocate conversion buffer\n");
            snd_pcm_close(pcm_handle);
            return -1;
        }
        
        // Convert audio
        if (audio->channels == 2 && device_channels == 1) {
            // Stereo to mono: mix left and right channels
            printf("Converting stereo to mono...\n");
            for (int i = 0; i < play_frames; i++) {
                int32_t mixed = (audio->samples[i * 2] + audio->samples[i * 2 + 1]) / 2;
                play_buffer[i] = (int16_t)mixed;
            }
        } else if (audio->channels == 1 && device_channels == 2) {
            // Mono to stereo: duplicate mono channel
            printf("Converting mono to stereo...\n");
            for (int i = 0; i < play_frames; i++) {
                play_buffer[i * 2] = audio->samples[i];
                play_buffer[i * 2 + 1] = audio->samples[i];
            }
        } else {
            printf("ERROR: Unsupported channel conversion (%d -> %d)\n", 
                   audio->channels, device_channels);
            free(play_buffer);
            snd_pcm_close(pcm_handle);
            return -1;
        }
    }
    
    // Play the audio
    int frames_remaining = play_frames;
    int offset = 0;
    
    while (frames_remaining > 0) {
        int chunk_frames = (frames_remaining > frames) ? frames : frames_remaining;
        
        pcm = snd_pcm_writei(pcm_handle, play_buffer + (offset * device_channels), chunk_frames);
        
        if (pcm == -EPIPE) {
            printf("XRUN occurred, recovering...\n");
            snd_pcm_prepare(pcm_handle);
        } else if (pcm < 0) {
            printf("ERROR: Can't write to PCM device. %s\n", snd_strerror(pcm));
            break;
        } else {
            offset += chunk_frames;
            frames_remaining -= chunk_frames;
        }
    }
    
    // Cleanup
    if (need_conversion) {
        free(play_buffer);
    }
    
    // Wait for playback to complete
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    
    printf("Playback finished.\n");
    return 0;
}

void free_audio_data(AudioData *audio) {
    if (audio->samples) {
        free(audio->samples);
        audio->samples = NULL;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <audio_file.wav>\n", argv[0]);
        printf("   or: %s <raw_file.raw> <sample_rate> <channels>\n", argv[0]);
        return 1;
    }
    
    AudioData audio = {0};
    
    // Check file extension to determine format
    char *ext = strrchr(argv[1], '.');
    if (ext && strcmp(ext, ".wav") == 0) {
        // WAV file
        if (read_wav_file(argv[1], &audio) != 0) {
            return 1;
        }
    } else {
        // Assume raw file - need sample rate and channels
        if (argc < 4) {
            printf("For raw files, specify: %s file.raw sample_rate channels\n", argv[0]);
            printf("Example: %s audio.raw 44100 2\n", argv[0]);
            return 1;
        }
        
        uint32_t sample_rate = atoi(argv[2]);
        uint16_t channels = atoi(argv[3]);
        
        if (read_raw_file(argv[1], &audio, sample_rate, channels) != 0) {
            return 1;
        }
    }
    
    // Play the audio
    if (play_audio_alsa(&audio) != 0) {
        free_audio_data(&audio);
        return 1;
    }
    
    free_audio_data(&audio);
    return 0;
}