;;; pd-remote.el --- Pd remote control helper

;; Copyright (c) 2023 Albert Graef

;; Author: Albert Graef <aggraef@gmail.com>
;; Keywords: multimedia, pure-data
;; Version: 1.1.1
;; Package-Requires: (faust-mode lua-mode)
;; URL: https://github.com/agraef/pd-remote
;; License: MIT

;;; Commentary:

;; pd-remote is a remote-control and live-coding utility for Pd and Emacs.

;; You can add this to your .emacs for remote control of Pd patches in
;; conjunction with the accompanying pd-remote.pd abstraction.  In particular,
;; there's built-in live-coding support for faust-mode and lua-mode via the
;; pd-faustgen2 and pd-lua externals.

;; Install this with the Emacs package manager or put it anywhere where Emacs
;; will find it, and load it in your .emacs as follows:

;; (require 'pd-remote)

;;; Code:

(defgroup pd-remote nil
  "Pd remote control helper."
  :prefix "pd-remote-"
  :group 'multimedia)

(defcustom pd-remote-pdsend "pdsend"
  "This variable specifies the pathname of the pdsend program."
  :type 'string
  :group 'pd-remote)

(defcustom pd-remote-port "4711"
  "This variable specifies the UDP port number to be used."
  :type 'string
  :group 'pd-remote)

(defun pd-remote-start-process ()
  "Start a pdsend process to communicate with Pd via UDP."
  (interactive)
  (start-process "pdsend" nil pd-remote-pdsend pd-remote-port "localhost" "udp")
  (set-process-query-on-exit-flag (get-process "pdsend") nil))

(defun pd-remote-stop-process ()
  "Stops a previously started pdsend process."
  (interactive)
  (delete-process "pdsend"))

;;;###autoload
(defun pd-remote-message (message)
  "Send the given MESSAGE to Pd.  Start the pdsend process if needed."
  (interactive "sMessage: ")
  (unless (get-process "pdsend") (pd-remote-start-process))
  (process-send-string "pdsend" (concat message "\n")))

;; some convenient helpers

;;;###autoload
(defun pd-remote-dsp-on ()
  "Start dsp processing."
  (interactive)
  (pd-remote-message "pd dsp 1"))

;;;###autoload
(defun pd-remote-dsp-off ()
  "Stop dsp processing."
  (interactive)
  (pd-remote-message "pd dsp 0"))

;; Faust mode; this requires Juan Romero's Faust mode available at
;; https://github.com/rukano/emacs-faust-mode. NOTE: If you don't have this,
;; or you don't need it, just comment the following two lines.
(setq auto-mode-alist (cons '("\\.dsp$" . faust-mode) auto-mode-alist))
(autoload 'faust-mode "faust-mode" "FAUST editing mode." t)

;; various convenient keybindings, factored out so that they can be used
;; in different keymaps
(defun pd-remote-keys (mode-map)
  "Add common Pd keybindings to MODE-MAP."
  (define-key mode-map "\C-c\C-q" #'pd-remote-stop-process)
  (define-key mode-map "\C-c\C-m" #'pd-remote-message)
  (define-key mode-map "\C-c\C-s" #'(lambda () "Start" (interactive)
				      (pd-remote-message "play 1")))
  (define-key mode-map "\C-c\C-t" #'(lambda () "Stop" (interactive)
				      (pd-remote-message "play 0")))
  (define-key mode-map "\C-c\C-r" #'(lambda () "Restart" (interactive)
				      (pd-remote-message "play 0")
				      (pd-remote-message "play 1")))
  (define-key mode-map [(control ?\/)] #'pd-remote-dsp-on)
  (define-key mode-map [(control ?\.)] #'pd-remote-dsp-off))

;; Juan's Faust mode doesn't have a local keymap, add one.
(defvar faust-mode-map nil)
(cond
 ((not faust-mode-map)
  (setq faust-mode-map (make-sparse-keymap))
  ;; Some convenient keybindings for Faust mode.
  (define-key faust-mode-map "\C-c\C-k" #'(lambda () "Compile" (interactive)
					    (pd-remote-message "faustgen2~ compile")))
  (pd-remote-keys faust-mode-map)))
(add-hook 'faust-mode-hook #'(lambda () (use-local-map faust-mode-map)))

;; Lua mode: This requires lua-mode from MELPA.
(require 'lua-mode)
;; Pd Lua uses this as the extension for Lua scripts
(setq auto-mode-alist (cons '("\\.pd_luax?$" . lua-mode) auto-mode-alist))
;; add some convenient key bindings
(define-key lua-mode-map "\C-c\C-c" #'lua-send-current-line)
(define-key lua-mode-map "\C-c\C-d" #'lua-send-defun)
(define-key lua-mode-map "\C-c\C-r" #'lua-send-region)
; Pd tie-in (see pd-lua tutorial)
(pd-remote-keys lua-mode-map)
(define-key lua-mode-map "\C-c\C-k" #'(lambda () "Reload" (interactive)
					(pd-remote-message "pdluax reload")))

;; add any convenient global keybindings here
;(global-set-key [(control ?\/)] #'pd-remote-dsp-on)
;(global-set-key [(control ?\.)] #'pd-remote-dsp-off)

(provide 'pd-remote)

;; End:
;;; pd-remote.el ends here
