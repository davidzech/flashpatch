#include <flash/flash.h>

constexpr FlashInfo MX29L010 = {.maxTime = mxMaxTime,
                                .type = {
                                    .romSize = 131072,
                                    .sector =
                                        {
                                            .size = 4096,
                                            .shift = 12,
                                            .count = 32,
                                            .top = 0,
                                        },
                                }};
