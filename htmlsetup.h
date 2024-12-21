String s =  "<!DOCTYPE html>\n"
            "<head>\n"
            "<title>Crypto Plate</title>\n"

            "<style type='text/css'>\n"

            "textarea {\n"
            "width: 300px;\n"
            "height: 2em;\n"
            "}\n"

            "textarea {\n"
            "  font-size: 100%;\n"
            "}\n"

            "t1 {\n"
            "  font-size: 100%;\n"
            "  font-weight: bold;\n"
            "}\n"

            "</style>\n" 
          

            "<script>\n"
            "function SubmitAll() { \n document.forms[0].submit();\n }"
            "</script>\n"
            "</head>\n"

            "<body>\n"

            "<form id='form1' action='/string/' method='get' target='_self'>\n"
            "SSID (32 chars max)<br><textarea autofocus maxlength = '32' name = 'input1'></textarea><br>\n" 
            "SSID Password (16 chars max)<br><textarea maxlength = '16' name = 'input2'></textarea><br>\n"
            "Refresh (in minutes - 3 integers max)<br><textarea maxlength = '3' name = 'input3'></textarea><br>\n"
            "Time Zone (hours ahead/behind UTC (e.g CST is -6 - 3 integers max)<br><textarea maxlength = '3' name = 'input4'></textarea><br>\n"  
            "</form>\n"

            "<input type='button' value='Submit' onClick='SubmitAll()'>\n"
            "\n"
            "</body>\n";
            