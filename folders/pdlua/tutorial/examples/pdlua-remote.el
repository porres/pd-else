;;; pdlua-remote.el --- Pd remote control stuff for pd-lua.

;;; Commentary:

;;; Install this anywhere where Emacs finds it (e.g., in the Emacs site-lisp
;;; directory -- usually under /usr/share/emacs/site-lisp on Un*x systems, or
;;; in any directory on the Emacs load-path) and load it in your .emacs as
;;; follows:

;;; (require 'pdlua-remote)

;;; Or just add the following lines directly to your .emacs.

;;; Code:

(defun pd-send-start-process ()
  "Start a pdsend process to communicate with Pd via UDP port 4711."
  (interactive)
  (start-process "pdsend" nil "pdsend" "4711" "localhost" "udp")
  (set-process-query-on-exit-flag (get-process "pdsend") nil))

(defun pd-send-stop-process ()
  "Stops a previously started pdsend process."
  (interactive)
  (delete-process "pdsend"))

(defun pd-send-message (message)
  "Send the given MESSAGE to Pd.  Start the pdsend process if needed."
  (interactive "sMessage: ")
  (unless (get-process "pdsend") (pd-send-start-process))
  (process-send-string "pdsend" (concat message "\n")))

; Pd Lua uses this as the extension for Lua scripts
(require 'lua-mode)
(setq auto-mode-alist (cons '("\\.pd_luax?$" . lua-mode) auto-mode-alist))
; Pd tie-in for lua-mode (see pd-lua tutorial)
(define-key lua-mode-map "\C-c\C-k" '(lambda () "Reload" (interactive)
				       (pd-send-message "reload")))

(provide 'pdlua-remote)
;;; pdlua-remote.el ends here
