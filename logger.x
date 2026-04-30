   struct log_strct {
      char uName[256];
      char op[256];
      char file[256];
   } ;

   program LOGGER {
      version LOGGERVER {
         int log ( struct log_strct ) = 1;
      } = 1;
   } = 1;