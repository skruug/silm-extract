//
//  wav.hpp
//  silm-extract
//
//  Created by Vadim Kindl on 30.12.2022.
//

#ifndef wav_h
#define wav_h

typedef struct wavfile_header_s
{
    char    ChunkID[4];     /*  4   */
    int32_t ChunkSize;      /*  4   */
    char    Format[4];      /*  4   */
    
    char    Subchunk1ID[4]; /*  4   */
    int32_t Subchunk1Size;  /*  4   */
    int16_t AudioFormat;    /*  2   */
    int16_t NumChannels;    /*  2   */
    int32_t sample_rate;     /*  4   */
    int32_t ByteRate;       /*  4   */
    int16_t BlockAlign;     /*  2   */
    int16_t BitsPerSample;  /*  2   */
    
    char    Subchunk2ID[4];
    int32_t Subchunk2Size;
} wavfile_header_t;


#define SUBCHUNK1SIZE   (16)
#define AUDIO_FORMAT    (1) /*For PCM*/
//#define NUM_CHANNELS    (2)
//#define SAMPLE_RATE     (44100)

//#define BITS_PER_SAMPLE (16)

// #define BYTE_RATE       (SAMPLE_RATE * NUM_CHANNELS * BITS_PER_SAMPLE / 8)
#define BLOCK_ALIGN     (NUM_CHANNELS * BITS_PER_SAMPLE / 8)


int write_wav_header(FILE *file_p, int32_t sample_rate, int32_t frame_count)
{
    int ret;
    
    wavfile_header_t wav_header;
    int32_t subchunk2_size;
    int32_t chunk_size;
    
    size_t write_count;
    
    int num_channels = 1;
    int bits_per_sample = 8;
    
    subchunk2_size  = frame_count * num_channels * bits_per_sample / 8;
    chunk_size      = 4 + (8 + SUBCHUNK1SIZE) + (8 + subchunk2_size);
    
    wav_header.ChunkID[0] = 'R';
    wav_header.ChunkID[1] = 'I';
    wav_header.ChunkID[2] = 'F';
    wav_header.ChunkID[3] = 'F';
    
    wav_header.ChunkSize = chunk_size;
    
    wav_header.Format[0] = 'W';
    wav_header.Format[1] = 'A';
    wav_header.Format[2] = 'V';
    wav_header.Format[3] = 'E';
    
    wav_header.Subchunk1ID[0] = 'f';
    wav_header.Subchunk1ID[1] = 'm';
    wav_header.Subchunk1ID[2] = 't';
    wav_header.Subchunk1ID[3] = ' ';
    
    wav_header.Subchunk1Size = SUBCHUNK1SIZE;
    wav_header.AudioFormat = AUDIO_FORMAT;
    wav_header.NumChannels = num_channels;
    wav_header.sample_rate = sample_rate;
    wav_header.ByteRate = (sample_rate * num_channels * bits_per_sample / 8);
    wav_header.BlockAlign = (num_channels * bits_per_sample / 8);
    wav_header.BitsPerSample = bits_per_sample;
    
    wav_header.Subchunk2ID[0] = 'd';
    wav_header.Subchunk2ID[1] = 'a';
    wav_header.Subchunk2ID[2] = 't';
    wav_header.Subchunk2ID[3] = 'a';
    wav_header.Subchunk2Size = subchunk2_size;
    
    write_count = fwrite(&wav_header, sizeof(wavfile_header_t), 1, file_p);
                    
    ret = (1 != write_count) ? -1 : 0;
    
    return ret;
}

#endif /* wav_h */
