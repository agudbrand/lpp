enum class DoubleDashArg
{
 Unknown,
 verbosity,
 start_lsp,
 help,
};
static DoubleDashArg parse_double_dash_arg(const char* arg)
{
 switch (*arg)
 {
  case 's':
   arg += 1;
   switch (*arg)
   {
    case 't':
     arg += 1;
     switch (*arg)
     {
      case 'a':
       arg += 1;
       switch (*arg)
       {
        case 'r':
         arg += 1;
         switch (*arg)
         {
          case 't':
           arg += 1;
           switch (*arg)
           {
            case '-':
             arg += 1;
             switch (*arg)
             {
              case 'l':
               arg += 1;
               switch (*arg)
               {
                case 's':
                 arg += 1;
                 switch (*arg)
                 {
                  case 'p':
                   arg += 1;
                   switch (*arg)
                   {
                    case '\0':
                     return DoubleDashArg::start_lsp;
                     break;
                   }
                   break;
                 }
                 break;
               }
               break;
             }
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 'v':
   arg += 1;
   switch (*arg)
   {
    case 'e':
     arg += 1;
     switch (*arg)
     {
      case 'r':
       arg += 1;
       switch (*arg)
       {
        case 'b':
         arg += 1;
         switch (*arg)
         {
          case 'o':
           arg += 1;
           switch (*arg)
           {
            case 's':
             arg += 1;
             switch (*arg)
             {
              case 'i':
               arg += 1;
               switch (*arg)
               {
                case 't':
                 arg += 1;
                 switch (*arg)
                 {
                  case 'y':
                   arg += 1;
                   switch (*arg)
                   {
                    case '\0':
                     return DoubleDashArg::verbosity;
                     break;
                   }
                   break;
                 }
                 break;
               }
               break;
             }
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 'h':
   arg += 1;
   switch (*arg)
   {
    case 'e':
     arg += 1;
     switch (*arg)
     {
      case 'l':
       arg += 1;
       switch (*arg)
       {
        case 'p':
         arg += 1;
         switch (*arg)
         {
          case '\0':
           return DoubleDashArg::help;
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
 }
 return DoubleDashArg::Unknown;
}
enum class SingleDashArg
{
 Unknown,
 v,
};
static SingleDashArg parse_single_dash_arg(const char* arg)
{
 switch (*arg)
 {
  case 'v':
   arg += 1;
   switch (*arg)
   {
    case '\0':
     return SingleDashArg::v;
     break;
   }
   break;
 }
 return SingleDashArg::Unknown;
}
enum class VerbosityOption
{
 Unknown,
 trace,
 debug,
 info,
 notice,
 warn,
 error,
 fatal,
};
static VerbosityOption parse_verbosity_option(const char* arg)
{
 switch (*arg)
 {
  case 'e':
   arg += 1;
   switch (*arg)
   {
    case 'r':
     arg += 1;
     switch (*arg)
     {
      case 'r':
       arg += 1;
       switch (*arg)
       {
        case 'o':
         arg += 1;
         switch (*arg)
         {
          case 'r':
           arg += 1;
           switch (*arg)
           {
            case '\0':
             return VerbosityOption::error;
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 'n':
   arg += 1;
   switch (*arg)
   {
    case 'o':
     arg += 1;
     switch (*arg)
     {
      case 't':
       arg += 1;
       switch (*arg)
       {
        case 'i':
         arg += 1;
         switch (*arg)
         {
          case 'c':
           arg += 1;
           switch (*arg)
           {
            case 'e':
             arg += 1;
             switch (*arg)
             {
              case '\0':
               return VerbosityOption::notice;
               break;
             }
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 't':
   arg += 1;
   switch (*arg)
   {
    case 'r':
     arg += 1;
     switch (*arg)
     {
      case 'a':
       arg += 1;
       switch (*arg)
       {
        case 'c':
         arg += 1;
         switch (*arg)
         {
          case 'e':
           arg += 1;
           switch (*arg)
           {
            case '\0':
             return VerbosityOption::trace;
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 'f':
   arg += 1;
   switch (*arg)
   {
    case 'a':
     arg += 1;
     switch (*arg)
     {
      case 't':
       arg += 1;
       switch (*arg)
       {
        case 'a':
         arg += 1;
         switch (*arg)
         {
          case 'l':
           arg += 1;
           switch (*arg)
           {
            case '\0':
             return VerbosityOption::fatal;
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 'w':
   arg += 1;
   switch (*arg)
   {
    case 'a':
     arg += 1;
     switch (*arg)
     {
      case 'r':
       arg += 1;
       switch (*arg)
       {
        case 'n':
         arg += 1;
         switch (*arg)
         {
          case '\0':
           return VerbosityOption::warn;
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 'i':
   arg += 1;
   switch (*arg)
   {
    case 'n':
     arg += 1;
     switch (*arg)
     {
      case 'f':
       arg += 1;
       switch (*arg)
       {
        case 'o':
         arg += 1;
         switch (*arg)
         {
          case '\0':
           return VerbosityOption::info;
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
  case 'd':
   arg += 1;
   switch (*arg)
   {
    case 'e':
     arg += 1;
     switch (*arg)
     {
      case 'b':
       arg += 1;
       switch (*arg)
       {
        case 'u':
         arg += 1;
         switch (*arg)
         {
          case 'g':
           arg += 1;
           switch (*arg)
           {
            case '\0':
             return VerbosityOption::debug;
             break;
           }
           break;
         }
         break;
       }
       break;
     }
     break;
   }
   break;
 }
 return VerbosityOption::Unknown;
}
static const char* cliargs_list_str = R"help_message(  --verbosity <level> set the verbosity <level> of the internal logger to one of: trace, debug, info, notice, warn, error, fatal
  --start-lsp         start the language server instead of invoking lpp.
  --help              display this help message.
  -v                  set the verbosity of the internal logger to 'debug'.
)help_message";
