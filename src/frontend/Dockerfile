FROM node:20-alpine AS build

WORKDIR /app

ARG REACT_APP_API_BASE=http://localhost:8000
ENV REACT_APP_API_BASE=$REACT_APP_API_BASE

COPY package.json package-lock.json ./
RUN npm install

COPY . .
RUN npm run build


FROM nginx:1.27-alpine AS production


COPY nginx.conf /etc/nginx/conf.d/default.conf

COPY --from=build /app/build /usr/share/nginx/html

EXPOSE 80

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD wget --quiet --tries=1 --spider http://localhost:80/ || exit 1

CMD ["nginx", "-g", "daemon off;"]

# ---------------------------------------------------------------------------
FROM node:20-alpine AS development

WORKDIR /app

COPY package.json package-lock.json ./
RUN npm install

COPY . .

ENV CI=true \
    WATCHPACK_POLLING=true

EXPOSE 3000

CMD ["npm", "start"]