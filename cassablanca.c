// Cassablanca Ceiling Fan Remote codes
#define CodeLength 23

unsigned int light_on[CodeLength]   = {1300, 400, 1250, 400, 450, 1200, 1300, 400, 1300, 350, 450, 1250, 1250, 400, 450, 1200, 450, 1250, 450, 1200, 450, 1250, 400};
unsigned int light_off[CodeLength]  = {1300, 400, 1250, 400, 450, 1250, 1250, 400, 1300, 350, 450, 1250, 450, 1200, 1300, 400, 450, 1200, 450, 1200, 450, 1250, 400};

unsigned int fan_off[CodeLength]    = {1350, 300, 1350, 350, 500, 1150, 1350, 350, 1300, 350, 500, 1150, 500, 1200, 450, 1200, 500, 1200, 450, 1200, 500, 1150, 1300};
unsigned int fan_high[CodeLength]   = {1350, 350, 1300, 350, 500, 1200, 1300, 350, 1300, 350, 450, 1250, 450, 1200, 450, 1200, 450, 1250, 450, 1200, 1350, 300, 500};
unsigned int fan_medium[CodeLength] = {1250, 400, 1250, 400, 450, 1250, 1250, 400, 1300, 350, 450, 1250, 450, 1200, 450, 1250, 450, 1200, 1300, 400, 450, 1200, 450};
unsigned int fan_low[CodeLength]    = {1300, 400, 1250, 400, 450, 1250, 1250, 400, 1300, 400, 400, 1250, 450, 1200, 450, 1250, 1250, 400, 450, 1200, 450, 1250, 450};