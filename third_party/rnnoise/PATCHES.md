# Changes to upstream RNNoise 0.2

* `src/denoise.c`, `include/rnnoise.h`: added `rnnoise_set_low_latency()`. When enabled the gains computed for a
  frame are applied to that frame instead of the previous one, which removes 480 samples (10 ms) of delay.
* `src/rnnoise_data.c`: contains only `init_rnnoise()`; the generated weight arrays were moved into
  `weights_blob.bin` (made with upstream's `write_weights`) and are loaded with `rnnoise_model_from_buffer()`.
