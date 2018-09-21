;;; Provide a skip-mode for emacs
;;;
;;; Add this to your .emacs file:
;;; (load "<path to skip>/skip/ide/emacs/skip.el")

;;; One construct that this indenter doesn't properly handle:
;;; | Foo _ as x ->
;;;   bar();
;;;   baz();
;;;
;;; Will be indented as:
;;; | Foo _ as x ->
;;;     bar();
;;; baz();
;;;
;;; Adding braces will correct this.



(require 'derived)
(require 'cc-mode)

;;; Debugging: Uncomment the (apply 'message e) line below and then indent your
;;; line.  Examine the message buffer using M-x view-echo-area-messages (C-h e).
(defun skip-debug (&rest e)
;;;  (apply 'message e)
  )



(defvar skip-keywords
  '("alias" "as" "async" "await" "catch" "children" "class" "concurrent"
    "const" "else" "extends" "false" "from" "fun" "if" "uses" "trait" "match"
    "module" "mutable" "native" "private" "protected" "public" "readonly"
    "static" "this" "throw" "true" "try" "type" "void" "watch" "with"
    "overridable" "memoized" "frozen" "deferred" "return"))


(defvar skip-tab-width 2 "Width of a tab for SKIP mode")


(defvar skip-font-lock-defaults
  `((
     ;; stuff between double quotes
     ("//.*" . font-lock-comment-face)
     ("\a\\.\\*\\?" . font-lock-string-face)
     ("\"\\.\\*\\?" . font-lock-string-face)
     ( ,(regexp-opt skip-keywords 'symbols) . font-lock-keyword-face)
     ("\\<[A-Z]+[a-zA-Z_0-9]*" . font-lock-type-face)
     )))


(defun is-block-open (tok) (or (string= tok "(") (string= tok "[")))
(defun is-block-close (tok) (or (string= tok ")") (string= tok "]")))


(defconst skip-2char-ops
  '("[" "]" "=>" "->" ">=" "<=" "==" "!=" "||" "::" "//" "&&" ";;"
    ;; We treat "/*" as a token so it's never misinterpreted as either "/" or "*"
    "/*"))


(defconst skip-continuation-before-tokens
  '("+" "-" "*" "/" "=" "->" "=>" "("))

(defconst skip-continuation-after-tokens
  '("+" "-" "*" "/" "="))


(defun char-is-word (ch)
  ;; it seems like there should be a better way to do this
  (and (integerp ch)
       (or
        (= ch ?_)
        (= ch ?-)   ;; So unary minus (no space after '-') indents properly
        (and (>= ch ?A) (<= ch ?Z))
        (and (>= ch ?a) (<= ch ?z))
        (and (>= ch ?0) (<= ch ?9)))))


(defun is-word (w) (string-match "^[[:word:]]+$" w))


(defun forward-syntactic-whitespace ()
  (c-skip-ws-forward)
  ;; If we're at a comment skip it and try again
  (cond
   ((looking-at-p "//")
    (end-of-line)
    (forward-syntactic-whitespace))
   ((looking-at-p "/\\*")
    (search-forward "*/")
    (forward-syntactic-whitespace))))


(defun forward-token ()
    (forward-syntactic-whitespace)
  (if (eobp)
      nil
    (let ((ch (char-after))
          (start (point)))

      (forward-char)
      (cond
       ;; identifier or number
       ((char-is-word ch)
        (while (and (not (eobp)) (char-is-word (char-after)))
          (forward-char)))

       ;; string
       ((eq ch ?\")
        (forward-char)
        (while (and (not (eobp))
                    (or (not (eq (char-after) ?\"))
                        (eq (char-before) ?\\)))
          (forward-char)))

       ;; two-char identifiers
       ((and (not (eobp)) (member (string ch (char-after)) skip-2char-ops))
        (forward-char))

       ;; everything else is just 1 char
       )

      (buffer-substring start (point)))))


(defun forward-token-same-line ()
  (let ((line-end (line-end-position))
        (start (point))
        (tok (forward-token)))
    (if (<= (point) line-end)
        tok
      (goto-char start)
      nil)))


(defun backward-token-same-line ()
  (let ((line-start (line-beginning-position))
        (start (point))
        (tok (backward-token)))
    (if (>= (point) line-start)
        tok
      (goto-char start)
      nil)))


(defun line-comment-start-location ()
  ;; TBD: This doesn't handle '//' in a string properly
  (save-excursion
    (beginning-of-line)
    (if (search-forward "//" (line-end-position) t)
        (- (point) 2)
      (line-end-position))))


(defun bsws-eol-comment-helper ()
  (let ((comment (line-comment-start-location)))
    (if (> (point) comment)
        (progn
          (goto-char comment)
          t)
      nil)))


(defun bsws-block-comment-helper ()
  (if (and (eq (char-before) ?/)
           (progn
             (backward-char)
             (eq (char-before) ?*)))
      (progn
        ;; This doesn't handle comments like /* /* */ properly!
        (search-backward "/*")
        t)))


(defun backward-syntactic-whitespace ()
  ;; If we're in the comment then move to the beginning of the comment
  (c-skip-ws-backward)

  (if (cond
       ;; See if we're in an EOL comment
       ((bsws-eol-comment-helper) t)

       ;; See if we're at the end of a block comment
       ((bsws-block-comment-helper) t))
      (backward-syntactic-whitespace)))




(defun backward-token ()
  (backward-syntactic-whitespace)

  ;; Because emacs' looking-back is non-greedy we need to hack this up ourselves.
  (if (bobp)
      nil
    (let ((ch (char-before))
          (end (point)))

      (backward-char)
      (cond
       ;; identifier or number
       ((char-is-word ch)
        (while (and (not (bobp)) (char-is-word (char-before)))
          (backward-char)))

       ;; string
       ((eq ch ?\")
        (backward-char)
        (while (and (not (bobp)) (or (not (eq (char-after) ?\")) (eq (char-before) ?\\)))
          (backward-char)))

       ;; two-char identifiers
       ((and (not (bobp)) (member (string (char-before) ch) skip-2char-ops))
        (backward-char))

       ;; everything else is just 1 char
       )

      (buffer-substring (point) end))))


(defun in-block-comment ()
  (let ((open (save-excursion
                (condition-case nil
                    (progn (search-backward "/*") (point))
                  (search-failed -1))))
        (close (save-excursion
                (condition-case nil
                    (progn (search-backward "*/") (point))
                  (search-failed -1)))))
    (> open close)))


(defun forward-token-if (tok)
  (let ((start (point)))
    (if (string= (forward-token) tok)
        t
      (goto-char start)
      nil)))


(defun forward-token-peek ()
  (save-excursion (forward-token)))


(defun parse-children-child ()
  ;; | Foo
  ;; | Foo(...)
  ;; | Foo{...}
  (if (forward-token-if "|")
      (if (is-word (forward-token))
          (let ((next (forward-token-peek)))
            (if (or (string= next "(") (string= next "{"))
                (forward-list))
            t))))


(defun in-children-block (parent-point)
  (save-excursion
    (let ((start (point)))
      (goto-char parent-point)
      (forward-token)
      (if (string= (forward-token) "children")
          (if (string= (forward-token) "=")
              (if (< (point) start)
                  (progn
                    (while (and
                            (< (point) start)
                            (parse-children-child)))
                    (< start (point)))))))))


(defun in-function-match (parent-point)
  ;; Scan backward to see if we're in a function definition.  We're basically
  ;; going backward at the same scope until we see ';' or 'fun'.
  (save-excursion
    ;; fun foo():String
    ;;   | Foo -> foo
    ;;   | Baz -> baz
    (let ((done nil))
      (while (not done)
        (cond
         ((or (< (point) parent-point)
              (eq (char-before) ?\;))
          (setq done 0))

         ((or (eq (char-before) ?\))
              (eq (char-before) ?\}))
          (backward-sexp))

         ((string= (backward-token) "fun")
          (setq done 1))))

      (if (eq done 1)
          (progn
            (back-to-indentation)
            (point))))))


(defun in-match (parent-point)
  ;; Scan backward to see if we're in a match.  It's harder than just looking
  ;; for "|" because it has to be in the same scope level.
  (let ((start (point))
        (found nil))
    (if (not (save-excursion (search-backward "|" parent-point t)))
        ;; There's no way we could be in a match
        nil
      ;; we might be in a match - scan backward until we find a match token
      (while (and (not found)
                  (> (point) parent-point))
        (let ((tok (backward-token)))
          (if (is-block-close tok)
              (progn
                (forward-char)
                (backward-sexp))
            (if (string= tok "|")
                (setq found t))))))
    (if (not found)
        (progn
          (goto-char start)
          nil)
      t)))


(defun backward-token-or-sexp ()
  (let ((tok (backward-token)))
    (if (is-block-close tok)
        (progn
          (forward-char)
          (backward-sexp)))
    tok))


(defun backward-token-same-line-or-sexp ()
  (let ((tok (backward-token-same-line)))
    (if (is-block-close tok)
        (progn
          (forward-char)
          (backward-sexp)))
    tok))


;;; Like back-to-indentation but skips sexps
(defun skip-beginning-of-statement ()
  (while (backward-token-same-line-or-sexp))
  (back-to-indentation)
  )


(defun skip-expression-is-closure ()
  (let ((tok (backward-token-or-sexp)))
    (if tok
        (cond
         ((string= tok "->") t)
         ((member tok '("{" "(" "[" ";" ",")) nil)
         (t (skip-expression-is-closure))))))


(defun skip-indent-line ()
  "Indent current line of skip code"
  (interactive)

  ;;; TODO: There are a lot of constants here which should be variables...
  (if (not (or
            (eq (line-beginning-position) (point-min))
            (in-block-comment)))
      (let ((indent nil))
        (save-excursion
          (let ((start-point (point))
                (parent-point (condition-case nil
                                  (progn
                                    (save-excursion
                                      (back-to-indentation)
                                      (backward-up-list)
                                      (point)))
                                (scan-error nil)))
                (parent-tok nil)
                (parent-line-beginning-position nil)
                (parent-line-ending-position nil)
                (parent-indent 0)
                (parent-indent-point nil)
                (parent-at-end nil)
                (parent-is-match-case nil)
                (cur-tok nil))

            (if parent-point
                (save-excursion
                  (goto-char parent-point)
                  (setq parent-tok (forward-token-peek))
                  (setq parent-line-ending-position (line-end-position))
                  (setq parent-at-end (eq (+ parent-point 1) parent-line-ending-position))

                  (setq parent-indent-point
                        (if (and (is-block-open parent-tok)
                                 (not parent-at-end))
                            (line-beginning-position)
                          (progn
                            (skip-beginning-of-statement)
                            (point))))
                  (setq parent-line-beginning-position
                        (progn (goto-char parent-indent-point) (line-beginning-position)))
                  (setq parent-indent (- parent-indent-point parent-line-beginning-position))
                  (setq parent-is-match-case
                        (progn
                          (goto-char parent-indent-point)
                          (eq (char-after) ?\|)))))

            (skip-debug "parent: pt:%s tok:%s line:%s-%s ind-pt:%s ind:%s"
                      parent-point parent-tok parent-line-beginning-position
                      parent-line-ending-position parent-indent-point parent-indent)

            ;; Go to the beginning of the current line - if we're at the top of the
            ;; buffer then just indent 0.
            (beginning-of-line)

            (setq cur-tok (save-excursion (forward-token-same-line)))

            ;; If the line starts with a close paren then we should line up
            ;; with the matching open paren.
            (unless indent
              (if (is-block-close cur-tok)
                  (if (eq (+ parent-point 1) parent-line-ending-position)
                      (progn
                        (setq indent parent-indent)
                        (skip-debug "rule 10a: %s" indent))
                    (setq indent (- parent-point parent-line-beginning-position))
                    (skip-debug "rule 10b: %s" indent))))

            ;; If the line starts with a close brace then we should line up
            ;; with the matching opening brace's line.
            ;; One extra nit - if the opening brace's line starts with '|'
            ;; then indent.
            (unless indent
              (if (string= cur-tok "}")
                  (if parent-is-match-case
                      (progn
                        ;; | Foo -> {
                        ;;   }
                        (setq indent (+ parent-indent 2))
                        (skip-debug "rule 20a: %s" indent))
                    ;; foo -> {
                    ;; }
                    (setq indent parent-indent)
                    (skip-debug "rule 20b: %s" indent))))

            ;; See if we're in one of the weird 'children =' sections.
            (unless indent
              (if (and (string= parent-tok "{")
                       (in-children-block parent-point))
                  (progn
                    ;; children =
                    ;;   | Foo
                    (setq indent (+ parent-indent 2))
                    (skip-debug "rule 25: %s" indent))))

            ;; If the line starts with '|' then we should line up indented off
            ;; the parent scope.
            (unless indent
              (if (string= cur-tok "|")
                  (progn
                    (let ((fn-point (in-function-match parent-point)))
                      (if fn-point
                          (progn
                            ;; fun foo():String
                            ;;   | Bar
                            (setq indent
                                  (- fn-point
                                     (save-excursion
                                       (goto-char fn-point)
                                       (line-beginning-position))))
                            (skip-debug "rule 30a: %s" indent))
                        ;; fun foo():String {
                        ;;   | Bar
                        (if parent-is-match-case
                            (progn
                              (setq indent (+ parent-indent 4))
                              (skip-debug "rule 30b: %s" indent))
                          (setq indent parent-indent)
                          (skip-debug "rule 30b: %s" indent)))))))

            ;; Line continuation
            (unless indent
              (let ((tok (backward-token)))
                (if (or
                     (member tok skip-continuation-before-tokens)
                     (member cur-tok skip-continuation-after-tokens))
                    (progn
                      (skip-debug "rule 50: %s %s" tok cur-tok)

                      ;; Special case - if the previous line starts with '|'
                      ;; then we want to indent the parent
                      (save-excursion
                        (skip-beginning-of-statement)
                        (if (string= (save-excursion (forward-token)) "|")
                            (progn
                              ;; | foo ->
                              ;;     bar
                              (forward-syntactic-whitespace)
                              (setq parent-indent (- (point) (line-beginning-position)))
                              (setq parent-is-match-case t)
                              (skip-debug "rule 50*: %s" parent-indent))))


                      (cond
                       ;; If we're in a paren and it's the EOL
                       ((and (is-block-open parent-tok) parent-at-end)
                        (if (save-excursion (goto-char start-point) (skip-expression-is-closure))
                            (progn
                              ;;; foo(
                              ;;;   a ->
                              ;;;     b)
                              (setq indent (+ parent-indent 4))
                              (skip-debug "rule 50ba: %s" indent))
                          ;;; foo(
                          ;;;   2)
                          (setq indent (+ parent-indent 2 (if parent-is-match-case 2 0)))
                          (skip-debug "rule 50bb: %s" indent)))

                       ;; We're in a paren but not EOL
                       ((is-block-open parent-tok)
                        (if (string= tok "->")
                            ;;; foo((x) ->
                            ;;;       bar)
                            (progn
                              (setq indent (- parent-point
                                              parent-line-beginning-position
                                              -3))
                              (skip-debug "rule 50c: %s" indent))

                          ;;; foo(a +
                          ;;;     b)
                          (setq indent (- parent-point
                                          parent-line-beginning-position
                                          -1))
                          (skip-debug "rule 50d: %s" indent)))

                       ;; Otherwise indent extra relative to the parent
                       (parent-point
                        ;;; foo() {
                        ;;;   a = 2 +
                        ;;;     3
                        ;;; }
                        (setq indent (+ parent-indent 4))
                        (skip-debug "rule 50e: %s" indent))

                       (t
                        ;;; a = 2 +
                        ;;;   3
                        (setq indent (+ parent-indent 2))
                        (skip-debug "rule 50f: %s" indent)))))))

            ;; If we're at the top level then just assume 0.
            (unless indent
              (unless parent-indent
                (setq indent 0)
                (skip-debug "rule 60: %s" indent)))

            ;; If the enclosing block is parenthesized...
            (unless indent
              (if (is-block-open parent-tok)
                  (progn
                    ;; If the enclosing block's '(' is EOL then indent
                    (if (eq (+ parent-point 1) parent-line-ending-position)
                        (progn
                          (setq indent (+ parent-indent 2))
                          (skip-debug "rule 70a: %s" indent))
                      (setq indent (- parent-point parent-line-beginning-position -1))
                      (skip-debug "rule 70b: %s" indent)))))

            ;; By default indent from the enclosing block.
            (unless indent
              (if parent-point
                  (if parent-is-match-case
                      (progn
                        ;; | Foo -> {
                        ;;     bar()
                        (setq indent (+ parent-indent 4))
                        (skip-debug "rule 90a: %s" indent))
                    (progn
                        ;; Foo -> {
                        ;;   bar()
                        (setq indent (+ parent-indent 2))
                        (skip-debug "rule 90c: %s" indent)))
                (progn
                  ;; No parent - top level construct
                  (setq indent 0)
                  (skip-debug "rule 90c: %s" indent))))
            ))
        (if (eq (line-beginning-position) (line-end-position))
            ;; blank line
            (indent-line-to indent)
          (save-excursion (indent-line-to indent)))

        ;; If the point is between the beginning of the line and the
        ;; indent then move up to the indent
        (if (< (- (point) (line-beginning-position)) indent)
            (goto-char (+ (line-beginning-position) indent)))
        )))


(defvar skip-mode-syntax-table
  (let ((table (make-syntax-table)))
    (c-populate-syntax-table table)
    ;; Treat backtick quoted strings as strings
    (modify-syntax-entry ?` "\"" table)
    table))


(define-derived-mode skip-mode prog-mode "Skip"
  "SKIP mode is a major mode for editing Skip files"

  ;; for comments
  (setq comment-end ""
        comment-start "//"
        font-lock-defaults skip-font-lock-defaults
        tab-width skip-tab-width
        indent-line-function 'skip-indent-line
        )
  (set-syntax-table skip-mode-syntax-table)
  (c-initialize-cc-mode t)
  (c-init-language-vars-for 'c++-mode)
  )


(defun default-skip-mode-hook ()
  (c-set-offset 'block-close '-)
  (c-set-offset 'arglist-intro '+)
  (setq show-trailing-whitespace t)
  (add-to-list 'write-file-functions 'delete-trailing-whitespace)
  )
(add-hook 'skip-mode-hook 'default-skip-mode-hook)


(setq auto-mode-alist
  (append
   '(("\\.sk$" . skip-mode))
   auto-mode-alist))


(provide 'skip-mode)
