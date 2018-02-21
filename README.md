# Extract Loudest Section

This is a simple tool to take .wav audio files, identify the loudest segment of a given length, and
then write out that segment as a new .wav file. I'm using this to do simple alignment on some
captured audio of people saying single words, where there are indeterminate gaps before and after
the word. Complex alignment of the kind used to go from spoken sentences to time codes has proven to
be not as reliable as I'd like on this task, so since my requirements are straightforward, and I
couldn't find a good equivalent in ffmpeg or sox, I've put this one together.

It works by going through the audio samples and calculating the absolute value of each
sample. This approximates the volume at that point. The desired length of the audio is specified
(currently hard-coded in main.cc as `desired_length_ms`), and the sum of all the volumes for a
window of that length at all possible positions in the audio's timeline is calculated. The window
that has the highest total volume is then written out as a new file.

For a visual explanation, here's some ASCII art showing the volume of an input audio file:

```

            *     
           ***   **  *
 *         **** **** *
**** * ** ************* * ** *
----------------------------------
0.0s            1.0s           2.0s
```

The goal is to identify the important section where somebody is talking, and ignore the preamble
and trailing parts which just contain background noise. Because this background noise isn't silence,
it's hard to use simple filters like `silenceremove` from ffmpeg. Instead, what we want to do is
identify the important section, which above is obviously around the 1.0s mark. Since we know we can
only pick a second of audio to output, the filter will try to fit as much of the high volume section
within that window as possible, like this:

```
         < one second  > 
         |  *          |
         | ***   **  * |
 *       | **** **** * |
**** * **|*************|* ** *
---------+-------------+----------
0.0s     |      1.0s   |       2.0s
```

The other parts will be cropped out, and just that section will be saved.

This tool isn't designed for general use or flexibility:

 - It only deals with mono 16-bit WAVs, since that's all I need.

 - It takes two command line arguments, the first is the glob for the .wavs to read (for example
"*/*.wav") and the second is the root of the output directory. Sub-directories one level deep
will be created, and output files will be placed with the same names in those directories under the
output root. This peculiar setup is so that it's easy for me to process my files of speech data.'

## Building

There's a Makefile for Linux and Xcode project for MacOS.
