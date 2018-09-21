;;; skip-printer.el --- Minor mode to format Skip code on file save

;; Version: 0.1.0

;; Copyright (c) 2014 The go-mode Authors. All rights reserved.
;; Portions Copyright (c) 2015-present, Facebook, Inc. All rights reserved.

;; Redistribution and use in source and binary forms, with or without
;; modification, are permitted provided that the following conditions are
;; met:

;; * Redistributions of source code must retain the above copyright
;; notice, this list of conditions and the following disclaimer.
;; * Redistributions in binary form must reproduce the above
;; copyright notice, this list of conditions and the following disclaimer
;; in the documentation and/or other materials provided with the
;; distribution.
;; * Neither the name of the copyright holder nor the names of its
;; contributors may be used to endorse or promote products derived from
;; this software without specific prior written permission.

;; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
;; "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
;; LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
;; A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
;; OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
;; SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
;; LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
;; DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
;; THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
;; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)

;; Author: James Long and contributors
;; Created: 9 April 2018
;; Url: https://github.com/skiplang/skip
;; Keywords: convenience wp edit skip

;; Based on prettier-js.el.

;; This file is not part of GNU Emacs.

;;; Commentary:
;; Formats your Skip code code using 'skip_printer' on file save.

;;; Code:

(defgroup skip-printer nil
  "Minor mode to format Skip code on file save"
  :group 'languages
  :prefix "skip-printer"
  :link '(url-link :tag "Repository" "https://github.com/skiplang/skip"))

(defcustom skip-printer-command "skip_printer"
  "The 'skip_printer' command."
  :type 'string
  :group 'skip-printer)

(defcustom skip-printer-args '()
  "List of args to send to skip_printer command."
  :type '(repeat string)
  :group 'skip-printer)

(defcustom skip-printer-show-errors 'buffer
    "Where to display skip_printer error output.
It can either be displayed in its own buffer, in the echo area, or not at all.
Please note that Emacs outputs to the echo area when writing
files and will overwrite skip_printer's echo output if used from inside
a `before-save-hook'."
    :type '(choice
            (const :tag "Own buffer" buffer)
            (const :tag "Echo area" echo)
            (const :tag "None" nil))
      :group 'skip-printer)

(defun skip-printer--goto-line (line)
  "Move cursor to line LINE."
  (goto-char (point-min))
    (forward-line (1- line)))

(defun skip-printer--apply-rcs-patch (patch-buffer)
  "Apply an RCS-formatted diff from PATCH-BUFFER to the current buffer."
  (let ((target-buffer (current-buffer))
        ;; Relative offset between buffer line numbers and line numbers
        ;; in patch.
        ;;
        ;; Line numbers in the patch are based on the source file, so
        ;; we have to keep an offset when making changes to the
        ;; buffer.
        ;;
        ;; Appending lines decrements the offset (possibly making it
        ;; negative), deleting lines increments it. This order
        ;; simplifies the forward-line invocations.
        (line-offset 0))
    (save-excursion
      (with-current-buffer patch-buffer
        (goto-char (point-min))
        (while (not (eobp))
          (unless (looking-at "^\\([ad]\\)\\([0-9]+\\) \\([0-9]+\\)")
            (error "Invalid rcs patch or internal error in skip-printer--apply-rcs-patch"))
          (forward-line)
          (let ((action (match-string 1))
                (from (string-to-number (match-string 2)))
                (len  (string-to-number (match-string 3))))
            (cond
             ((equal action "a")
              (let ((start (point)))
                (forward-line len)
                (let ((text (buffer-substring start (point))))
                  (with-current-buffer target-buffer
                    (setq line-offset (- line-offset len))
                    (goto-char (point-min))
                    (forward-line (- from len line-offset))
                    (insert text)))))
             ((equal action "d")
              (with-current-buffer target-buffer
                (skip-printer--goto-line (- from line-offset))
                (setq line-offset (+ line-offset len))
                (let ((beg (point)))
                  (forward-line len)
                  (delete-region (point) beg))))
             (t
              (error "Invalid rcs patch or internal error in skip-printer--apply-rcs-patch")))))))))

(defun skip-printer--process-errors (filename errorfile errbuf)
  "Process errors for FILENAME, using an ERRORFILE and display the output in ERRBUF."
  (with-current-buffer errbuf
    (if (eq skip-printer-show-errors 'echo)
        (progn
          (message "%s" (buffer-string))
          (skip-printer--kill-error-buffer errbuf))
      (insert-file-contents errorfile nil nil nil)
      ;; Convert the skip_printer stderr to something understood by the compilation mode.
      (goto-char (point-min))
      (insert "skip_printer errors:\n")
      (compilation-mode)
      (display-buffer errbuf))))

(defun skip-printer--kill-error-buffer (errbuf)
  "Kill buffer ERRBUF."
  (let ((win (get-buffer-window errbuf)))
    (if win
        (quit-window t win)
      (with-current-buffer errbuf
        (erase-buffer))
      (kill-buffer errbuf))))

(defun skip-printer ()
   "Format the current buffer according to the skip_printer tool."
   (interactive)
   (let* ((ext (file-name-extension buffer-file-name t))
          (bufferfile (make-temp-file "skip_printer" nil ext))
          (outputfile (make-temp-file "skip_printer" nil ext))
          (errorfile (make-temp-file "skip_printer" nil ext))
          (errbuf (if skip-printer-show-errors (get-buffer-create "*skip_printer errors*")))
          (patchbuf (get-buffer-create "*skip_printer patch*"))
          (coding-system-for-read 'utf-8)
          (coding-system-for-write 'utf-8))
     (unwind-protect
         (save-restriction
           (widen)
           (write-region nil nil bufferfile)
           (if errbuf
               (with-current-buffer errbuf
                 (setq buffer-read-only nil)
                 (erase-buffer)))
           (with-current-buffer patchbuf
             (erase-buffer))
           (if (zerop (apply 'call-process
                             skip-printer-command bufferfile (list (list :file outputfile) errorfile)
                             nil
                             (append (list "--stdin-filepath" buffer-file-name)
                                     skip-printer-args)))
               (progn
                 (call-process-region (point-min) (point-max) "diff" nil patchbuf nil "-n" "--strip-trailing-cr" "-"
                                      outputfile)
                 (skip-printer--apply-rcs-patch patchbuf)
                 (message "Applied skip_printer with args `%s'" skip-printer-args)
                 (if errbuf (skip-printer--kill-error-buffer errbuf)))
             (message "Could not apply skip_printer")
             (if errbuf
                 (skip-printer--process-errors (buffer-file-name) errorfile errbuf))
             ))
       (kill-buffer patchbuf)
       (delete-file errorfile)
       (delete-file bufferfile)
       (delete-file outputfile))))

;;;###autoload
(define-minor-mode skip-printer-mode
  "Runs skip_printer on file save when this mode is turned on"
  :lighter " SkipPrinter"
  :global nil
  (if skip-printer-mode
      (add-hook 'before-save-hook 'skip-printer nil 'local)
    (remove-hook 'before-save-hook 'skip-printer 'local)))

(defun skip-printer-hook ()
  (skip-printer-mode 1))
(add-hook 'skip-mode-hook 'skip-printer-hook)


(provide 'skip-printer)
;;; skip-printer.el ends here
