#include <stdio.h>
#include <string.h>
#include <sok.h>

char *received(void *ptr, char *buffer, size_t len, size_t *res_size)
{
	char response[] =
		"<html><head><meta charset=\"UTF-8\"></head>"
			"<body style=\"padding:0;margin:20px;text-align:center;background-color:gray;\">"
				"<div class=\"container\" style=\"padding:20px 50px 20px 50px;text-align:left;width:80%;height:100%;background-color:gray;margin:auto;box-shadow:0px 0px 20px 3px rgba(0,0,0,0.5);\">"
					"<h1>Sistemas Distribuídos</h1>"
					"<ul>"
						"<li>Java</li>"
						"<li>Java SE</li>"
						"<li>Java EE</li>"
					"</ul>"
				"</div>"
			"</body>"
		"</html>";
	*res_size = sizeof(response);
	return strdup(response);
}

int main(int argc, char **argv)
{
	SSOK_Server *serv = SSOK_Server_new(80, NULL, received, NULL);
	SSOK_Server_run(serv, 0);
	SSOK_Server_destroy(serv);
	return 0;
}
