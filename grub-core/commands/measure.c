#include <grub/dl.h>
#include <grub/err.h>
#include <grub/extcmd.h>
#include <grub/file.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/tpm.h>

GRUB_MOD_LICENSE ("GPLv3+")

static grub_err_t
measure_file (unsigned long pcr, const char* filepath)
{
  grub_err_t rc = GRUB_ERR_NONE;
  grub_file_t file = NULL;
  grub_off_t filesize;
  grub_uint8_t *filebuf = NULL;

  if (!filepath)
    filepath = "";

  grub_dprintf("measure", "measure file \"%s\" to PCR %ul\n", filepath, pcr);

  file = grub_file_open (filepath, GRUB_FILE_TYPE_TO_HASH);
  if (!file) {
    rc = grub_error (GRUB_ERR_FILE_NOT_FOUND, N_("file not found: %s"), filepath);
    goto end;
  }

  filesize = grub_file_size (file);
  if (filesize == GRUB_FILE_SIZE_UNKNOWN) {
    rc = grub_error (GRUB_ERR_FILE_READ_ERROR, N_("file size unknown: %s"), filepath);
    goto end;
  }

  filebuf = grub_malloc (filesize);
  if (!filebuf) {
    rc = grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("file larger than memory: %s"), filepath);
    goto end;
  }

  if (grub_file_read (file, filebuf, filesize) != filesize) {
    rc = grub_error (GRUB_ERR_FILE_READ_ERROR, N_("file read failed: %s"), filepath);
    goto end;
  }

  rc = grub_tpm_measure ((unsigned char*)filebuf, filesize, pcr, "measure_command:file");
  if (rc != GRUB_ERR_NONE) {
    rc = grub_error (rc, N_("Measure file failed: %s"), filepath);
  }

end:

  if (filebuf)
    grub_free (filebuf);

  if (file)
    grub_file_close (file);

  return rc;
}

static grub_err_t
measure_string (unsigned long pcr, const char* str)
{
  grub_err_t rc;

  if (!str)
    str = "";

  grub_dprintf("measure", "measure string \"%s\" to PCR %ul\n", str, pcr);

  rc = grub_tpm_measure ((unsigned char*)str, grub_strlen(str), pcr, "measure_command:string");
  if (rc != GRUB_ERR_NONE)
    rc = grub_error (rc, N_("Measure string failed"));

  return rc;
}

static grub_err_t
measure_command (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  unsigned long pcr = 8; /* first static OS PCR */
  grub_err_t (*measure_fn)(unsigned long, const char*) = &measure_string;
  grub_err_t rc, temp_rc;
  int i;

  if (state[0].set) {
    pcr = grub_strtoul (state[0].arg, NULL, 0);
    if (pcr > 255)
      return grub_error (GRUB_ERR_OUT_OF_RANGE, N_("PCR register number be in range [0, 255]"));
  }

  if (state[1].set) {
      measure_fn = &measure_file;
  }

  rc = GRUB_ERR_NONE;

  /* process all args, keep last error code */
  for (i = 0; i < argc; ++i) {
    temp_rc = measure_fn (pcr, args[i]);
    if (temp_rc != GRUB_ERR_NONE) {
      rc = temp_rc;
      grub_error_push(); /* push and clear errno for next iteration */
    }
  }

  if (rc != GRUB_ERR_NONE)
    grub_print_error();

  return rc;
}

static const struct grub_arg_option options[] =
{
  {"pcr", 'p', 0, N_("PCR number (defaults to 8)."), 0, ARG_TYPE_INT},
  {"file", 'f', 0, N_("Arguments are file paths. Measures file contents instead of strings."), 0, ARG_TYPE_NONE},
  {0, 0, 0, 0, 0, 0}
};

static grub_command_t command;

GRUB_MOD_INIT(measure)
{
  command = grub_register_extcmd (
              "measure", measure_command, 0,
              N_("[-p pcrno] [-f] arg1 arg2 ... argN"),
              N_("Measures strings or file contents to PCR register. Empty strings/files do not change PCR state."),
              &options);
}

GRUB_MOD_FINI(measure)
{
  grub_unregister_extcmd (command);
}
