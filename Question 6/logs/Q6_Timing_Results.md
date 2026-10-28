# Question 6

I used the PlatformIO serial monitor with timestamps to record the output from my Question 5 program. These results are from `device-monitor-261021-171704.log`.

|Task|Time between runs|Average time|Regular intervals?|
|-|-|-|-|
|Task 1|About 5, 5 and 10 seconds, repeating|6.577 s|No|
|Task 2|About 20 seconds|19.995 s|Yes, approximately|
|Task 3|About 20 seconds|19.995 s|Yes, approximately|
|Task 4|About 20 seconds|19.995 s|Yes, approximately|
|Task 5|Did not appear in the output|Cannot calculate|Cannot determine|

To find the average, I subtracted the first timestamp from the last timestamp for each task and divided by the number of intervals.

For example, Task 1 appeared 39 times, so there were 38 intervals:

Average = (17:21:17.813 - 17:17:07.871) / 38

Average = 249.942 / 38 = 6.577 seconds

Tasks 2, 3 and 4 ran at approximately regular intervals of 20 seconds. Task 1 had a repeating pattern of about 5, 5 and 10 seconds between runs, so its intervals were not constant. Its average for this recording was 6.577 seconds. This differs from the expected long-term average of about 6.667 seconds because the recording ended partway through the repeating pattern.

Task 5 did not appear in the recording, so I could not calculate its average interval. In the original textbook example, Task 5 is defined but is not included in the task table.

These measurements are approximate because the timestamps show when the computer received the serial messages.

