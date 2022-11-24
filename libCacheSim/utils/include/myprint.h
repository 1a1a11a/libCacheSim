#pragma once

//
// Created by Juncheng Yang on 6/19/20.
//

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief a wrapper for printing progress, it will skip printing if 
 * print too fast or the progress is not changed
 * 
 * @param perc 
 */
void print_progress(double perc); 


#ifdef __cplusplus
}
#endif
